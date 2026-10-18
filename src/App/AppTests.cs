using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace FlydigiTriggerBridge {
    internal static class AppTests {
        private static int assertions;
        private static void Require(bool condition,string message){assertions++;if(!condition)throw new Exception(message);}
        internal static int Run() {
            var json=new JavaScriptSerializer();var state=new SessionState();
            Action<string> apply=s=>state.Apply(json.Deserialize<Dictionary<string,object>>(s));
            apply("{\"event\":\"session-started\",\"detail\":{\"mode\":\"Bridge\"}}");Require(state.Phase==BridgePhase.Preparing&&!state.Finished,"Start must be nonterminal");
            apply("{\"event\":\"connection-ready\",\"generation\":1}");
            apply("{\"event\":\"game-feedback\",\"percent\":[1,37,61,100]}");Require(state.Motors[0]==1&&state.Motors[1]==37&&state.Motors[2]==61&&state.Motors[3]==100,"UI changed game amplitude");
            apply("{\"event\":\"connection-lost\"}");Require(state.Phase==BridgePhase.Waiting&&Array.TrueForAll(state.Motors,v=>v==0),"Offline state must clear motor display");
            apply("{\"event\":\"connection-ready\",\"generation\":2}");Require(state.Generation==2&&Array.TrueForAll(state.Motors,v=>v==0),"Reconnect must not replay stale motor display");
            apply("{\"event\":\"profile-restored\"}");Require(!state.Finished&&state.Phase==BridgePhase.Restoring,"Profile restoration alone must not permit another session");
            apply("{\"event\":\"session-finished\",\"detail\":{\"profileRestored\":true,\"visibilityRestored\":true,\"serviceRestored\":true,\"needsRecovery\":false}}");Require(state.Finished&&state.Phase==BridgePhase.Idle,"Complete verified cleanup must return to idle");
            apply("{\"event\":\"session-finished\",\"detail\":{\"profileRestored\":false,\"visibilityRestored\":true,\"serviceRestored\":false,\"needsRecovery\":true}}");Require(state.Phase==BridgePhase.RecoveryRequired,"Offline stop must retain recovery state");
            bool rejected=false;try{apply("{\"event\":\"game-feedback\",\"percent\":[0,0,0,101]}");}catch(FormatException){rejected=true;}Require(rejected,"Invalid signal accepted");
            Require(CommandLine.Quote("a b")=="\"a b\"","Space quoting failed");
            Require(CommandLine.Quote("C:\\folder\\")=="\"C:\\folder\\\\\"","Trailing slash quoting failed");
            Require(CommandLine.Quote("a\"b")=="\"a\\\"b\"","Quote escaping failed");
            var setup=new SessionState();setup.Apply(json.Deserialize<Dictionary<string,object>>("{\"event\":\"session-started\",\"detail\":{\"mode\":\"SetupWinUHid\"}}"));setup.Apply(json.Deserialize<Dictionary<string,object>>("{\"event\":\"setup-restart-required\",\"detail\":\"Restart required\"}"));setup.Apply(json.Deserialize<Dictionary<string,object>>("{\"event\":\"session-finished\",\"detail\":{\"profileRestored\":true,\"visibilityRestored\":true,\"serviceRestored\":true}}"));Require(setup.RestartRequired&&setup.Message.Contains("Restart Windows"),"Installer restart notice was discarded");
            using(var shell=GamepadCanvas.Shell()){
                Require(shell.IsVisible(109,74)&&shell.IsVisible(198,99)&&shell.IsVisible(282,74)&&shell.IsVisible(371,99),"Trigger panels must be inside the shell");
                Require(shell.IsVisible(184,200)&&shell.IsVisible(299,203)&&shell.IsVisible(371,146),"Face controls exceed shell clearance");
            }
            string file=Path.GetTempFileName();try{
                var tail=new JsonTail();File.WriteAllText(file,"{\"event\":\"test\",",new UTF8Encoding(false));Require(new List<string>(tail.Read(file)).Count==0,"Partial journal line parsed too early");
                File.AppendAllText(file,"\"message\":\"ready\"}\n",new UTF8Encoding(false));Require(new List<string>(tail.Read(file)).Count==1,"Completed journal line missing");Require(new List<string>(tail.Read(file)).Count==0,"Journal line replayed");
                File.WriteAllText(file,"{}\n",new UTF8Encoding(false));Require(new List<string>(tail.Read(file)).Count==1,"Truncated journal not detected");
            }finally{File.Delete(file);}
            Console.WriteLine("App tests passed: "+assertions+" assertions. No controller, driver or service access.");return 0;
        }
    }
}
