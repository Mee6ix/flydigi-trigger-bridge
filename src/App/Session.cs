using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace FlydigiTriggerBridge {
    internal enum BridgePhase { Idle, Preparing, Active, Waiting, Restoring, RecoveryRequired, Failed }
    internal sealed class SessionState {
        internal BridgePhase Phase=BridgePhase.Idle;
        internal int[] Motors=new int[4];
        internal int LeftTrigger,RightTrigger,InputChanges,Generation;
        internal string Firmware="Not connected",LastError="",Message="Switch on your controller and start the bridge.";
        internal bool Finished,Restored,VisibilityRestored,ServiceRestored,RestartRequired;
        internal string Mode="";
        internal readonly Dictionary<string,bool> Requirements=new Dictionary<string,bool>();
        internal static string Text(IDictionary<string,object> row,string key){object value;return row.TryGetValue(key,out value)&&value!=null?Convert.ToString(value,System.Globalization.CultureInfo.InvariantCulture):"";}
        internal static bool Flag(IDictionary<string,object> row,string key){object value;return row.TryGetValue(key,out value)&&value is bool&&(bool)value;}
        internal static IDictionary<string,object> Detail(IDictionary<string,object> row){object value;return row.TryGetValue("detail",out value)&&value is IDictionary<string,object>?(IDictionary<string,object>)value:new Dictionary<string,object>();}
        internal void Apply(IDictionary<string,object> row) {
            string name=Text(row,"event");var detail=Detail(row);
            switch(name) {
                case "session-started":Mode=Text(detail,"mode");Phase=BridgePhase.Preparing;Message="Checking your setup...";Finished=false;LastError="";break;
                case "requirement":Requirements[Text(detail,"name")]=Flag(detail,"ready");break;
                case "requirements-checked":Message=Flag(detail,"ready")?"Your setup is ready.":"Setup needs attention. Open the Setup page.";break;
                case "setup-installing":Message=Text(row,"detail");break;
                case "setup-skipped":Message=Text(row,"detail");break;
                case "setup-restart-required":RestartRequired=true;Message=Text(row,"detail");break;
                case "validating-virtual-controller":Message="Validating four output channels. Keep the test window active.";break;
                case "pausing-service":Message="Preparing the controller connection...";break;
                case "physical-controller-hidden":Message="Connecting the virtual Xbox controller...";break;
                case "connecting-controller":Message="Switch on the controller through the dongle.";break;
                case "firmware":Firmware=Text(row,"version");break;
                case "connection-ready":Phase=BridgePhase.Active;Generation=Convert.ToInt32(row["generation"]);Message="Ready. Launch your game and play.";break;
                case "connection-lost":Phase=BridgePhase.Waiting;Array.Clear(Motors,0,4);LeftTrigger=RightTrigger=0;Message="Controller is asleep or disconnected. Switch it back on.";break;
                case "game-feedback":
                    object value;if(row.TryGetValue("percent",out value)&&value is IEnumerable){int i=0;foreach(object item in (IEnumerable)value){int level=Convert.ToInt32(item);if(i>=4||level<0||level>100)throw new FormatException("Invalid motor percentage");Motors[i++]=level;}if(i!=4)throw new FormatException("Four motor channels are required");}break;
                case "forwarded-input":InputChanges++;LeftTrigger=(int)Math.Round(Convert.ToDouble(row["leftTrigger"])*100);RightTrigger=(int)Math.Round(Convert.ToDouble(row["rightTrigger"])*100);break;
                case "native-error":LastError=Text(row,"message");break;
                case "session-error":LastError=Text(row,"detail");break;
                case "profile-restored":Restored=true;Phase=BridgePhase.Restoring;Message="Original profile restored. Finishing cleanup...";break;
                case "visibility-restored":VisibilityRestored=true;break;
                case "service-restored":ServiceRestored=true;break;
                case "recovery-required":Phase=BridgePhase.RecoveryRequired;Message=Text(row,"detail");break;
                case "session-finished":
                    Finished=true;Restored=Flag(detail,"profileRestored");VisibilityRestored=Flag(detail,"visibilityRestored");ServiceRestored=Flag(detail,"serviceRestored");Array.Clear(Motors,0,4);
                    if(Flag(detail,"needsRecovery")){Phase=BridgePhase.RecoveryRequired;Message="Original profile needs recovery. Switch on the same controller.";}
                    else if(!string.IsNullOrEmpty(LastError)||!VisibilityRestored||!ServiceRestored){Phase=BridgePhase.Failed;Message=string.IsNullOrEmpty(LastError)?"Cleanup needs attention. See Activity for details.":LastError;}
                    else {Phase=BridgePhase.Idle;if(Mode=="Requirements"){}else if(Mode=="ValidateSetup")Message="Installer package verified. No Windows changes.";else if(Mode.StartsWith("Setup",StringComparison.Ordinal)||Mode=="RemoveWinUHid")Message=RestartRequired?"Setup completed. Restart Windows, then check setup.":"Setup finished. Run Check setup before starting the bridge.";else Message="Stopped. Original controller state restored.";}break;
            }
        }
    }
    internal sealed class JsonTail {
        private long position;private readonly Decoder decoder=Encoding.UTF8.GetDecoder();private string pending="";
        internal IEnumerable<string> Read(string path) {
            var result=new List<string>();if(!File.Exists(path))return result;
            using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                if(stream.Length<position){position=0;pending="";decoder.Reset();}
                stream.Position=position;byte[] buffer=new byte[65536];int count;int budget=1024*1024;
                while(budget>0&&(count=stream.Read(buffer,0,Math.Min(buffer.Length,budget)))>0){char[] chars=new char[Encoding.UTF8.GetMaxCharCount(count)];int used=decoder.GetChars(buffer,0,count,chars,0,false);pending+=new string(chars,0,used);position+=count;budget-=count;}
            }
            int end;while((end=pending.IndexOf('\n'))>=0){string line=pending.Substring(0,end).Trim('\r','\uFEFF');pending=pending.Substring(end+1);if(line.Length>0)result.Add(line);}
            if(pending.Length>2*1024*1024)throw new IOException("A journal entry is too large");return result;
        }
    }
    internal sealed class SessionManager:IDisposable {
        [System.Runtime.InteropServices.DllImport("user32.dll")]private static extern bool AllowSetForegroundWindow(int processId);
        internal readonly string DataRoot,RuntimeRoot;
        internal string SessionDir="",Mode="";
        internal SessionState State=new SessionState();
        internal readonly List<string> Activity=new List<string>();
        internal int ActivityRevision;
        internal bool StopRequested,DiagnosticLogging;
        private Process process;private EventWaitHandle stop;
        private List<FileStream> runtimeLocks=new List<FileStream>();
        private JsonTail nativeTail=new JsonTail(),sessionTail=new JsonTail();
        private readonly JavaScriptSerializer json=new JavaScriptSerializer();
        internal bool Running{get{return process!=null&&!process.HasExited;}}
        internal bool Installing{get{return Mode.StartsWith("Setup",StringComparison.Ordinal)||Mode=="RemoveWinUHid";}}
        internal bool NeedsRecovery{get{return !Running&&File.Exists(Path.Combine(DataRoot,"pending-restore.json"));}}
        internal SessionManager(string data,string runtime){DataRoot=data;RuntimeRoot=runtime;if(NeedsRecovery){State.Phase=BridgePhase.RecoveryRequired;State.Message="Restore the original profile before starting a new session.";}}
        internal void Start(string mode,int duration) {
            if(Running)throw new InvalidOperationException("A session is already running");
            if(NeedsRecovery&&mode!="Recover"&&mode!="Requirements")throw new InvalidOperationException("Restore the original profile first");
            if(process!=null){process.Dispose();process=null;}if(stop!=null){stop.Dispose();stop=null;}
            ReleaseRuntimeLocks();runtimeLocks=Program.LockTrustedRuntime(RuntimeRoot);
            var oldRequirements=new Dictionary<string,bool>(State.Requirements);State=new SessionState();foreach(var pair in oldRequirements)State.Requirements[pair.Key]=pair.Value;
            State.Phase=BridgePhase.Preparing;State.Message="Preparing the session...";StopRequested=false;Mode=mode;
            string id=Guid.NewGuid().ToString("N");SessionDir=Path.Combine(DataRoot,"sessions",DateTime.UtcNow.ToString("yyyyMMddTHHmmssfffZ")+"-"+id.Substring(0,8));Directory.CreateDirectory(SessionDir);
            string eventName="Local\\FlydigiBridge.Stop."+id;stop=new EventWaitHandle(false,EventResetMode.ManualReset,eventName);
            nativeTail=new JsonTail();sessionTail=new JsonTail();
            string[] args={"-NoProfile","-ExecutionPolicy","Bypass","-File",Path.Combine(RuntimeRoot,"session.ps1"),"-Mode",mode,"-RuntimeRoot",RuntimeRoot,"-DataRoot",DataRoot,"-SessionDir",SessionDir,"-StopEvent",eventName,"-ParentPid",Process.GetCurrentProcess().Id.ToString(),"-DurationSeconds",duration.ToString()};
            string command=CommandLine.Join(args)+(DiagnosticLogging?" -DiagnosticLogging":"");
            var info=new ProcessStartInfo(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","powershell.exe"),command){UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden,WorkingDirectory=RuntimeRoot};
            info.EnvironmentVariables["PSModulePath"]=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),"System32","WindowsPowerShell","v1.0","Modules")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"WindowsPowerShell","Modules");
            try{process=Process.Start(info);AllowSetForegroundWindow(process.Id);Log("Session started: "+mode);}catch{stop.Dispose();stop=null;ReleaseRuntimeLocks();State.Phase=BridgePhase.Failed;throw;}
        }
        internal void RequestStop(){if(!Running||stop==null)return;if(Installing){State.Message="Waiting for the installer to finish. Complete or cancel its own window.";Log(State.Message);return;}StopRequested=true;stop.Set();State.Phase=BridgePhase.Restoring;State.Message="Stopping and restoring the original controller state...";Array.Clear(State.Motors,0,4);Log("Stop requested. Waiting for verified restoration.");}
        internal void Poll() {
            if(string.IsNullOrEmpty(SessionDir))return;
            foreach(string line in nativeTail.Read(Path.Combine(SessionDir,"engine.jsonl")))Apply(line,false);
            foreach(string line in sessionTail.Read(Path.Combine(SessionDir,"session.jsonl")))Apply(line,true);
            if(process!=null&&process.HasExited&&!State.Finished){State.Finished=true;Array.Clear(State.Motors,0,4);State.Phase=NeedsRecovery?BridgePhase.RecoveryRequired:BridgePhase.Failed;State.Message="The session ended unexpectedly. See Activity for details.";Log(State.Message);}
            if(StopRequested&&State.Finished&&!NeedsRecovery&&State.Restored&&State.ServiceRestored&&State.VisibilityRestored){State.Phase=BridgePhase.Idle;State.Message="Stopped. Original controller state restored.";}
            if(StopRequested&&!State.Finished){State.Phase=BridgePhase.Restoring;State.Message="Stopping and restoring the original controller state...";Array.Clear(State.Motors,0,4);}
            if(process!=null&&process.HasExited)ReleaseRuntimeLocks();
        }
        private void Apply(string line,bool lifecycle) {
            try{var row=json.Deserialize<Dictionary<string,object>>(line);State.Apply(row);
                string name=SessionState.Text(row,"event");if(lifecycle||name=="connection-ready"||name=="connection-lost"||name=="firmware"||name=="native-error"){
                    string detail=SessionState.Text(row,"detail");if(detail.StartsWith("System.Collections"))detail="";
                    Log(name.Replace('-',' ')+(detail.Length>0?": "+detail:""));}
            }catch(Exception e){State.LastError="Could not read session status: "+e.Message;Log(State.LastError);}
        }
        internal void Log(string text){ActivityRevision++;Activity.Add(DateTime.Now.ToString("HH:mm:ss")+"  "+text);if(Activity.Count>600)Activity.RemoveRange(0,Activity.Count-600);}
        private void ReleaseRuntimeLocks(){foreach(var stream in runtimeLocks)stream.Dispose();runtimeLocks.Clear();}
        public void Dispose(){if(Running)RequestStop();if(process!=null)process.Dispose();if(stop!=null)stop.Dispose();ReleaseRuntimeLocks();}
    }
}
