using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Threading;
using System.Windows.Forms;
using System.Collections.Generic;

[assembly: AssemblyTitle("Flydigi Trigger Bridge")]
[assembly: AssemblyDescription("Independent four-channel rumble over the Flydigi dongle")]
[assembly: AssemblyCompany("Flydigi Trigger Bridge contributors")]
[assembly: AssemblyVersion("0.1.2.0")]
[assembly: AssemblyFileVersion("0.1.2.0")]
namespace FlydigiTriggerBridge {
    internal static class Program {
        internal const string Version="0.1.2";
        private static readonly Dictionary<string,string> trustedFiles=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        [STAThread] private static int Main(string[] args) {
            System.Globalization.CultureInfo.DefaultThreadCurrentUICulture=new System.Globalization.CultureInfo("en-US");
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            try {
                if(Array.IndexOf(args,"--self-test")>=0) return AppTests.Run();
                int render=Array.IndexOf(args,"--render");
                if(render>=0) {
                    if(render+1>=args.Length)throw new ArgumentException("A preview image path is required");
                    using(var view=new MainWindow(null,true)) { view.Show();int page=Array.IndexOf(args,"--page");if(page>=0&&page+1<args.Length)view.ShowPreviewPage(args[page+1]);Application.DoEvents();
                        using(var bitmap=new System.Drawing.Bitmap(view.Width,view.Height)) {view.DrawToBitmap(bitmap,new System.Drawing.Rectangle(0,0,view.Width,view.Height));bitmap.Save(args[render+1]);}
                        view.Close(); }
                    return 0;
                }
                if(!new WindowsPrincipal(WindowsIdentity.GetCurrent()).IsInRole(WindowsBuiltInRole.Administrator)) {
                    try {Process.Start(new ProcessStartInfo(Application.ExecutablePath,CommandLine.Join(args)){UseShellExecute=true,Verb="runas",WindowStyle=ProcessWindowStyle.Normal});}
                    catch(System.ComponentModel.Win32Exception e) {if(e.NativeErrorCode==1223)MessageBox.Show("Administrator access was cancelled. The controller and service were not changed.","Flydigi Trigger Bridge",MessageBoxButtons.OK,MessageBoxIcon.Information);else throw;}
                    return 0;
                }
                // Stable data and mutex identities retain earlier recovery records and prevent
                // the old and renamed applications from running hardware sessions together.
                string data=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FlydigiBridge");
                int root=Array.IndexOf(args,"--data-root");if(root>=0&&root+1<args.Length)data=Path.GetFullPath(args[root+1]);
                bool created;using(var single=new Mutex(true,"Local\\FlydigiTriggerBridge.Application",out created)) {
                    if(!created){MessageBox.Show("Flydigi Trigger Bridge is already running. Open it from the system tray.","Flydigi Trigger Bridge");return 0;}
                    Directory.CreateDirectory(data);string runtime=ExtractRuntime(data);
                    if(Array.IndexOf(args,"--smoke-test")>=0||Array.IndexOf(args,"--setup-package-test")>=0||Array.IndexOf(args,"--setup-skip-test")>=0){using(var probe=new SessionManager(data,runtime)){bool setup=Array.IndexOf(args,"--setup-package-test")>=0,skip=Array.IndexOf(args,"--setup-skip-test")>=0;probe.Start(skip?"SetupExistingCheck":setup?"ValidateSetup":"Requirements",0);var until=DateTime.UtcNow.AddSeconds(120);while(probe.Running&&DateTime.UtcNow<until){probe.Poll();Thread.Sleep(100);}probe.Poll();File.WriteAllText(Path.Combine(data,skip?"setup-skip-test.json":setup?"setup-package-test.json":"smoke-test.json"),new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new{finished=probe.State.Finished,phase=probe.State.Phase.ToString(),error=probe.State.LastError,requirements=probe.State.Requirements,activity=probe.Activity}));return probe.State.Finished&&string.IsNullOrEmpty(probe.State.LastError)?0:1;}}
                    using(var session=new SessionManager(data,runtime))using(var view=new MainWindow(session,false)){
                        bool lifecycle=Array.IndexOf(args,"--lifecycle-test")>=0;
                        if(lifecycle)view.ConfigureLifecycleCheck(Path.Combine(data,"lifecycle-test.json"));
                        Application.Run(view);if(lifecycle)return view.LifecyclePassed?0:1;
                    }
                }
                return 0;
            } catch(Exception e) {if(Array.IndexOf(args,"--self-test")>=0||Array.IndexOf(args,"--smoke-test")>=0||Array.IndexOf(args,"--setup-package-test")>=0||Array.IndexOf(args,"--setup-skip-test")>=0){Console.Error.WriteLine(e.Message);return 1;}MessageBox.Show(e.Message,"Flydigi Trigger Bridge",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
        }
        private static string ExtractRuntime(string data) {
            byte[] payload;using(var input=Assembly.GetExecutingAssembly().GetManifestResourceStream("FlydigiTriggerBridge.Runtime.zip")) {
                if(input==null)throw new IOException("Packaged runtime is missing. Build the application with build.ps1.");
                using(var output=new MemoryStream()){input.CopyTo(output);payload=output.ToArray();}
            }
            string key;using(var sha=SHA256.Create())key=BitConverter.ToString(sha.ComputeHash(payload)).Replace("-","").ToLowerInvariant();
            string runtime=Path.Combine(data,"runtime",key.Substring(0,16));Directory.CreateDirectory(runtime);
            using(var input=new MemoryStream(payload))using(var zip=new ZipArchive(input,ZipArchiveMode.Read)) {
                string prefix=Path.GetFullPath(runtime)+Path.DirectorySeparatorChar;
                foreach(var entry in zip.Entries) {
                    string target=Path.GetFullPath(Path.Combine(runtime,entry.FullName.Replace('/',Path.DirectorySeparatorChar)));
                    if(!target.StartsWith(prefix,StringComparison.OrdinalIgnoreCase))throw new IOException("Invalid packaged file path");
                    if(entry.FullName.EndsWith("/")){Directory.CreateDirectory(target);continue;}
                    byte[] bytes;using(var stream=entry.Open())using(var copy=new MemoryStream()){stream.CopyTo(copy);bytes=copy.ToArray();}
                    bool matches=File.Exists(target)&&Equal(File.ReadAllBytes(target),bytes);
                    if(!matches){Directory.CreateDirectory(Path.GetDirectoryName(target));File.WriteAllBytes(target,bytes);}
                    using(var sha=SHA256.Create())trustedFiles[target]=BitConverter.ToString(sha.ComputeHash(bytes));
                }
            }
            return runtime;
        }
        private static bool Equal(byte[] a,byte[] b){if(a.Length!=b.Length)return false;for(int i=0;i<a.Length;i++)if(a[i]!=b[i])return false;return true;}
        internal static List<FileStream> LockTrustedRuntime(string root){
            var names=new List<string>{"FlydigiBridgeEngine.exe","FlydigiBridgeEngine.exe.sha256","session.ps1","hidhide-session.ps1","setup.ps1"};
            string driverPrefix=Path.Combine(root,"drivers")+Path.DirectorySeparatorChar;
            foreach(var path in trustedFiles.Keys)if(path.StartsWith(driverPrefix,StringComparison.OrdinalIgnoreCase))names.Add(path.Substring(root.Length+1));
            var streams=new List<FileStream>();try{foreach(string name in names){
                string path=Path.Combine(root,name),expected;if(!trustedFiles.TryGetValue(path,out expected))throw new IOException("Runtime does not match the embedded package");
                var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read);streams.Add(stream);using(var sha=SHA256.Create())if(BitConverter.ToString(sha.ComputeHash(stream))!=expected)throw new IOException("A packaged runtime file was modified. Restart the application to restore it.");}
                return streams;}catch{foreach(var stream in streams)stream.Dispose();throw;}
        }
    }
    internal static class CommandLine {
        internal static string Quote(string value) {
            if(value.IndexOf('\0')>=0)throw new ArgumentException("Command arguments cannot contain NUL");
            var result=new System.Text.StringBuilder("\"");int slashes=0;
            foreach(char c in value){if(c=='\\'){slashes++;continue;}if(c=='\"'){result.Append('\\',slashes*2+1);result.Append(c);slashes=0;continue;}result.Append('\\',slashes);slashes=0;result.Append(c);}
            result.Append('\\',slashes*2);result.Append('"');return result.ToString();
        }
        internal static string Join(string[] args){return string.Join(" ",Array.ConvertAll(args,Quote));}
    }
}
