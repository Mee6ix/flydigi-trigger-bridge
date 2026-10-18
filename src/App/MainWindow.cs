using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Windows.Forms;

namespace FlydigiTriggerBridge {
    internal sealed class MainWindow:Form {
        private readonly SessionManager session;private readonly bool demo;
        private readonly Panel host=new Panel();private readonly Dictionary<string,Panel> pages=new Dictionary<string,Panel>();private readonly Dictionary<string,Button> nav=new Dictionary<string,Button>();
        private Label title,subtitle,pill,status,description,firmware,footer,inputCount;
        private ActionButton start,stop,recover,test,check,validate;
        private ActionButton packageCheck,removeDriver;
        private readonly Dictionary<string,ActionButton> installButtons=new Dictionary<string,ActionButton>();
        private bool lifecycleConfigured;
        private readonly Meter[] motors=new Meter[4];private Meter leftInput,rightInput;private GamepadCanvas pad;
        private RichTextBox activity;private readonly Dictionary<string,Label> requirementLabels=new Dictionary<string,Label>();
        private NotifyIcon tray;private Timer timer;private CheckBox minimize,detailed;private bool closeAfterCleanup;private int activityCount=-1;private DateTime started;
        internal bool LifecyclePassed;
        internal void ShowPreviewPage(string name){if(!demo||!pages.ContainsKey(name))throw new ArgumentException("Unknown preview page");ShowPage(name);}
        internal void ConfigureLifecycleCheck(string output){
            lifecycleConfigured=true;
            bool launched=false,active=false,stopClicked=false;DateTime activeSince=DateTime.MaxValue,deadline=DateTime.UtcNow.AddSeconds(150);
            var probe=new Timer{Interval=250};
            Shown+=(s,e)=>BeginInvoke(new Action(()=>{Activate();start.PerformClick();launched=true;probe.Start();}));
            probe.Tick+=(s,e)=>{
                session.Poll();
                if(session.State.Phase==BridgePhase.Active&&!active){active=true;activeSince=DateTime.UtcNow;}
                if(launched&&session.Running&&!stopClicked&&((active&&DateTime.UtcNow>=activeSince.AddSeconds(5))||DateTime.UtcNow>=deadline)){stop.PerformClick();stopClicked=session.StopRequested;}
                if(launched&&!session.Running){
                    probe.Stop();probe.Dispose();
                    LifecyclePassed=active&&stopClicked&&session.State.Finished&&session.State.Restored&&session.State.VisibilityRestored&&session.State.ServiceRestored&&!session.NeedsRecovery;
                    File.WriteAllText(output,new System.Web.Script.Serialization.JavaScriptSerializer().Serialize(new{passed=LifecyclePassed,active=active,stopButtonUsed=stopClicked,phase=session.State.Phase.ToString(),profileRestored=session.State.Restored,visibilityRestored=session.State.VisibilityRestored,serviceRestored=session.State.ServiceRestored,needsRecovery=session.NeedsRecovery,error=session.State.LastError,session=session.SessionDir}));Close();
                }
            };
        }
        internal MainWindow(SessionManager manager,bool preview) {
            session=manager;demo=preview;Text="Flydigi Trigger Bridge";ClientSize=new Size(1160,780);MinimumSize=new Size(1080,760);StartPosition=FormStartPosition.CenterScreen;
            BackColor=Theme.Back;ForeColor=Theme.Text;Font=Theme.Font(10);AutoScaleMode=AutoScaleMode.Dpi;
            Icon=Icon.ExtractAssociatedIcon(Application.ExecutablePath);
            BuildLayout();BuildDashboard();BuildSetup();BuildActivity();BuildAbout();ShowPage("Dashboard");
            if(demo){status.Text="Ready to play";description.Text="Your game controls all four motors, independently.";pill.Text="DEMO PREVIEW";motors[0].Value=32;motors[1].Value=61;motors[2].Value=28;motors[3].Value=84;pad.Motors=new[]{32,61,28,84};leftInput.Value=18;rightInput.Value=72;firmware.Text="Firmware 6.9.5.9  /  Xbox One virtual controller";footer.Text="Preview only — no controller or service access.";return;}
            LoadSettings();BuildTray();timer=new Timer{Interval=200};timer.Tick+=(s,e)=>TickSession();timer.Start();UpdateView();
            Resize+=(s,e)=>{if(WindowState==FormWindowState.Minimized&&minimize.Checked){Hide();tray.ShowBalloonTip(1600,"Flydigi Trigger Bridge","The bridge continues running in the system tray.",ToolTipIcon.Info);}};
            FormClosing+=OnClosingSession;
            Shown+=(s,e)=>BeginInvoke(new Action(()=>{if(!lifecycleConfigured&&!session.Running)Begin("Requirements",0);}));
        }
        private Label Label(string text,float size,Color color,FontStyle style=FontStyle.Regular){return new Label{Text=text,Font=Theme.Font(size,style),ForeColor=color,BackColor=Color.Transparent,AutoSize=false};}
        private ActionButton Button(string text,int width,bool primary,Action action){var b=new ActionButton{Text=text,Width=width,Primary=primary,Margin=new Padding(0,0,10,0)};b.Click+=(s,e)=>{try{action();}catch(Exception ex){ShowError(ex.Message);}};return b;}
        private void BuildLayout() {
            var side=new Panel{Dock=DockStyle.Left,Width=202,BackColor=Theme.Panel};Controls.Add(side);
            var brand=Label("FLYDIGI\nTRIGGER\nBRIDGE",19,Theme.Text,FontStyle.Bold);brand.SetBounds(25,24,170,105);side.Controls.Add(brand);
            var byline=Label("Four motors. One connection.",8,Theme.Muted);byline.SetBounds(26,139,170,22);side.Controls.Add(byline);
            string[] names={"Dashboard","Setup","Activity","About"};for(int i=0;i<names.Length;i++){string page=names[i];var b=new Button{Text=page,FlatStyle=FlatStyle.Flat,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(16,0,0,0),Font=Theme.Font(10),ForeColor=Theme.Muted,BackColor=Theme.Panel,Cursor=Cursors.Hand};b.FlatAppearance.BorderSize=0;b.SetBounds(14,174+i*52,174,44);b.Click+=(s,e)=>ShowPage(page);side.Controls.Add(b);nav[page]=b;}
            var version=Label("v"+Program.Version+"  •  WINDOWS x64\nUnofficial community project",8,Theme.Muted);version.Dock=DockStyle.Bottom;version.Height=69;version.Padding=new Padding(25,0,0,0);side.Controls.Add(version);
            var main=new Panel{Dock=DockStyle.Fill,Padding=new Padding(30,24,30,15)};Controls.Add(main);main.BringToFront();
            var header=new Panel{Dock=DockStyle.Top,Height=83};title=Label("Dashboard",24,Theme.Text,FontStyle.Bold);title.SetBounds(0,0,420,43);subtitle=Label("Independent trigger rumble over your wireless dongle.",10,Theme.Muted);subtitle.SetBounds(1,48,620,23);header.Controls.Add(title);header.Controls.Add(subtitle);
            pill=Label("BRIDGE OFF",9,Theme.Accent,FontStyle.Bold);pill.Dock=DockStyle.Right;pill.Width=173;pill.TextAlign=ContentAlignment.TopRight;pill.Padding=new Padding(0,15,0,0);header.Controls.Add(pill);
            footer=Label("Ready when you are.",8,Theme.Muted);footer.Dock=DockStyle.Bottom;footer.Height=28;footer.TextAlign=ContentAlignment.BottomLeft;
            host.Dock=DockStyle.Fill;main.Controls.Add(host);main.Controls.Add(footer);main.Controls.Add(header);
        }
        private Panel NewPage(string name){var p=new Panel{Dock=DockStyle.Fill,BackColor=Theme.Back};pages[name]=p;host.Controls.Add(p);return p;}
        private void BuildDashboard() {
            var page=NewPage("Dashboard");var layout=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=4,BackColor=Theme.Back};layout.RowStyles.Add(new RowStyle(SizeType.Absolute,264));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,161));layout.RowStyles.Add(new RowStyle(SizeType.Absolute,111));layout.RowStyles.Add(new RowStyle(SizeType.Percent,100));page.Controls.Add(layout);
            var connection=new Card{Dock=DockStyle.Fill,Margin=new Padding(0,0,0,14)};layout.Controls.Add(connection,0,0);
            var left=new Panel{Dock=DockStyle.Left,Width=337,BackColor=Theme.Panel};connection.Controls.Add(left);
            var small=Label("CONTROLLER CONNECTION",8,Theme.Muted,FontStyle.Bold);small.SetBounds(0,0,330,22);left.Controls.Add(small);
            status=Label("Ready to bridge",23,Theme.Text,FontStyle.Bold);status.SetBounds(0,38,328,76);left.Controls.Add(status);
            description=Label("Switch on your controller, then start the bridge.",10,Theme.Muted);description.SetBounds(1,118,329,40);left.Controls.Add(description);
            var identity=Label("Flydigi Vader 4 Pro · Wireless dongle",9,Theme.Text);identity.SetBounds(1,164,330,20);left.Controls.Add(identity);
            firmware=Label("Firmware will appear after connection",8,Theme.Muted);firmware.SetBounds(1,188,337,14);left.Controls.Add(firmware);
            pad=new GamepadCanvas{Dock=DockStyle.Fill};connection.Controls.Add(pad);pad.BringToFront();
            var outputs=new Card{Dock=DockStyle.Fill,Margin=new Padding(0,0,0,14),Padding=new Padding(21,18,21,17)};layout.Controls.Add(outputs,0,1);
            var outputTitle=Label("GAME RUMBLE",8,Theme.Muted,FontStyle.Bold);outputTitle.Dock=DockStyle.Top;outputTitle.Height=34;outputs.Controls.Add(outputTitle);
            var channelRow=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=4,RowCount=1,BackColor=Theme.Panel};string[] labels={"LEFT GRIP","RIGHT GRIP","LEFT TRIGGER","RIGHT TRIGGER"};for(int i=0;i<4;i++){channelRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,25));motors[i]=new Meter{Dock=DockStyle.Fill,Title=labels[i],Accent=i<2?Theme.Cyan:Theme.Accent,Margin=new Padding(0,0,i<3?23:0,0)};channelRow.Controls.Add(motors[i],i,0);}outputs.Controls.Add(channelRow);channelRow.BringToFront();
            var input=new Card{Dock=DockStyle.Fill,Margin=new Padding(0,0,0,14),Padding=new Padding(21,16,21,12)};layout.Controls.Add(input,0,2);
            var inputRow=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=3,RowCount=1,BackColor=Theme.Panel};inputRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,33));inputRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,33));inputRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,34));input.Controls.Add(inputRow);
            leftInput=new Meter{Dock=DockStyle.Fill,Title="LT INPUT",Accent=Theme.Muted,Margin=new Padding(0,0,22,0)};rightInput=new Meter{Dock=DockStyle.Fill,Title="RT INPUT",Accent=Theme.Muted,Margin=new Padding(0,0,22,0)};inputRow.Controls.Add(leftInput,0,0);inputRow.Controls.Add(rightInput,1,0);inputCount=Label("INPUT EVENTS\n0",10,Theme.Muted);inputCount.Dock=DockStyle.Fill;inputCount.TextAlign=ContentAlignment.MiddleLeft;inputRow.Controls.Add(inputCount,2,0);
            var actions=new FlowLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(0,6,0,0),WrapContents=true};layout.Controls.Add(actions,0,3);
            start=Button("Start bridge",154,true,()=>Begin("Bridge",0));stop=Button("Stop & restore",152,false,()=>session.RequestStop());test=Button("Test four motors",164,false,()=>Begin("Test",30));recover=Button("Restore profile",148,false,()=>Begin("Recover",0));recover.Visible=false;actions.Controls.Add(start);actions.Controls.Add(stop);actions.Controls.Add(test);actions.Controls.Add(recover);
            var note=Label("Game amplitudes pass through unchanged. The original profile is restored on stop.",8,Theme.Muted);note.Width=780;note.Height=31;note.Margin=new Padding(0,13,0,0);actions.Controls.Add(note);
        }
        private void BuildSetup() {
            var page=NewPage("Setup");page.AutoScroll=true;var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,FlowDirection=FlowDirection.TopDown,WrapContents=false};page.Controls.Add(flow);
            string[] names={"Virtual controller driver","Microsoft GameInput","HidHide","Flydigi transport driver"};string[] explanations={"WinUHid provides the virtual Xbox controller with four rumble channels.","The official Microsoft runtime exposes modern controller input and rumble.","Hides the original dongle while the bridge is active, then restores visibility.","The supported Space Station transport driver must already be installed."};
            string[] components={"WinUHid","GameInput","HidHide","SpaceStation"};
            for(int i=0;i<names.Length;i++){
                string component=components[i];
                var card=new Card{Width=856,Height=102,Margin=new Padding(0,0,0,10),Padding=new Padding(18,12,18,12)};
                var name=Label(names[i],11,Theme.Text,FontStyle.Bold);name.SetBounds(18,12,540,24);card.Controls.Add(name);
                var explanation=Label(explanations[i],9,Theme.Muted);explanation.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;explanation.SetBounds(18,43,591,44);card.Controls.Add(explanation);
                var ready=Label("NOT CHECKED",8,Theme.Muted,FontStyle.Bold);ready.SetBounds(650,10,182,22);ready.Anchor=AnchorStyles.Top|AnchorStyles.Right;ready.TextAlign=ContentAlignment.MiddleRight;card.Controls.Add(ready);requirementLabels[names[i]]=ready;
                var install=Button(component=="SpaceStation"?"Get Space Station":"Install",168,false,()=>InstallComponent(component));install.SetBounds(664,44,168,37);install.Anchor=AnchorStyles.Top|AnchorStyles.Right;card.Controls.Add(install);installButtons[names[i]]=install;
                flow.Controls.Add(card);
            }
            var buttons=new FlowLayoutPanel{Width=856,Height=65,Padding=new Padding(0,6,0,0)};check=Button("Check setup",142,true,()=>Begin("Requirements",0));validate=Button("Check four channels",193,false,()=>Begin("VirtualCheck",0));buttons.Controls.Add(check);buttons.Controls.Add(validate);buttons.Controls.Add(Button("Setup guide",140,false,ShowSetupGuide));flow.Controls.Add(buttons);
            var installers=new FlowLayoutPanel{Width=856,Height=59};packageCheck=Button("Verify installer package",223,false,()=>Begin("ValidateSetup",0));removeDriver=Button("Remove virtual driver",210,false,()=>InstallComponent("RemoveWinUHid"));installers.Controls.Add(packageCheck);installers.Controls.Add(removeDriver);flow.Controls.Add(installers);
            var settings=new Card{Width=856,Height=141,Padding=new Padding(18,14,18,14)};var heading=Label("BEHAVIOR",8,Theme.Muted,FontStyle.Bold);heading.SetBounds(18,12,650,23);settings.Controls.Add(heading);
            minimize=new CheckBox{Text="Minimize to the system tray",Checked=true,ForeColor=Theme.Text,BackColor=Theme.Panel,Font=Theme.Font(10)};minimize.SetBounds(18,47,450,25);settings.Controls.Add(minimize);detailed=new CheckBox{Text="Detailed diagnostic logs (next session)",ForeColor=Theme.Text,BackColor=Theme.Panel,Font=Theme.Font(10)};detailed.SetBounds(18,81,510,25);settings.Controls.Add(detailed);minimize.CheckedChanged+=(s,e)=>SaveSettings();detailed.CheckedChanged+=(s,e)=>{if(session!=null)session.DiagnosticLogging=detailed.Checked;SaveSettings();};flow.Controls.Add(settings);
            page.Resize+=(s,e)=>{int width=Math.Max(720,page.ClientSize.Width-5);flow.Width=width;foreach(Control c in flow.Controls)c.Width=width;};
        }
        private void BuildActivity() {
            var page=NewPage("Activity");var actions=new FlowLayoutPanel{Dock=DockStyle.Top,Height=63};actions.Controls.Add(Button("Open log folder",168,false,()=>OpenFolder(Path.Combine(session.DataRoot,"sessions"))));actions.Controls.Add(Button("Export diagnostics",182,false,ExportDiagnostics));page.Controls.Add(actions);
            activity=new RichTextBox{Dock=DockStyle.Fill,ReadOnly=true,BackColor=Theme.Panel,ForeColor=Theme.Muted,BorderStyle=BorderStyle.None,Font=new Font("Consolas",9),DetectUrls=false,WordWrap=true};page.Controls.Add(activity);activity.BringToFront();
        }
        private void BuildAbout() {
            var page=NewPage("About");var card=new Card{Dock=DockStyle.Top,Height=367};page.Controls.Add(card);var heading=Label("Your dongle. All four motors.",23,Theme.Text,FontStyle.Bold);heading.SetBounds(24,22,770,44);card.Controls.Add(heading);
            var copy=Label("Flydigi Trigger Bridge  "+Program.Version+"\n\nA Windows application for independent grip and trigger rumble over the Flydigi wireless dongle. Games see a virtual Xbox controller; their four motor values are forwarded unchanged.\n\nValidated hardware: Vader 4 Pro, model 105, firmware 6.9.5.9. Other firmware versions are refused until verified.\n\nThis is an unofficial community project. It is not affiliated with Flydigi or Microsoft. No firmware flashing or persistent profile saving is performed.",11,Theme.Muted);copy.SetBounds(24,83,770,214);card.Controls.Add(copy);
            var actions=new FlowLayoutPanel{Location=new Point(24,302),Size=new Size(770,47),BackColor=Theme.Panel};actions.Controls.Add(Button("Read documentation",201,false,()=>ShowDocument("README.md","Documentation")));actions.Controls.Add(Button("Third-party notices",191,false,()=>ShowDocument("THIRD_PARTY_NOTICES.md","Third-party notices")));card.Controls.Add(actions);
        }
        private void ShowPage(string name){foreach(var pair in pages)pair.Value.Visible=pair.Key==name;foreach(var pair in nav){pair.Value.ForeColor=pair.Key==name?Theme.Accent:Theme.Muted;pair.Value.BackColor=pair.Key==name?Color.FromArgb(37,49,46):Theme.Panel;}title.Text=name;subtitle.Text=name=="Dashboard"?"Independent trigger rumble over your wireless dongle.":name=="Setup"?"Check the components needed to connect your controller.":name=="Activity"?"Session progress, restoration and diagnostic details.":"Built for independent rumble, with your game in control.";}
        private void Begin(string mode,int duration){if(demo)return;session.Start(mode,duration);started=DateTime.Now;UpdateView();}
        private void InstallComponent(string component){
            if(demo)return;if(session.Running||session.NeedsRecovery)throw new InvalidOperationException("Stop and restore the bridge before changing setup.");
            if(component=="SpaceStation"){Process.Start(new ProcessStartInfo("https://www.flydigi.com/"){UseShellExecute=true});return;}
            if(!InstallationDialog.Confirm(this,component,session.RuntimeRoot))return;
            Begin(component=="RemoveWinUHid"?"RemoveWinUHid":"Setup"+component,0);
        }
        private void TickSession(){try{session.Poll();UpdateView();if(closeAfterCleanup&&!session.Running){closeAfterCleanup=false;Close();}}catch(Exception e){session.Log(e.Message);footer.Text="Status update failed: "+e.Message;}}
        private void UpdateView() {
            if(demo)return;var state=session.State;bool running=session.Running;bool pending=session.NeedsRecovery;
            start.Enabled=!running&&!pending;test.Enabled=!running&&!pending;stop.Enabled=running&&!session.StopRequested&&!session.Installing;check.Enabled=!running;validate.Enabled=!running&&!pending;recover.Visible=pending;recover.Enabled=!running;
            packageCheck.Enabled=!running&&!pending;
            removeDriver.Visible=File.Exists(Path.Combine(session.DataRoot,"installation","winuhid.json"));removeDriver.Enabled=!running&&!pending;
            foreach(var item in installButtons){bool ready;bool known=session.State.Requirements.TryGetValue(item.Key,out ready);item.Value.Enabled=!running&&!pending&&(item.Key=="Flydigi transport driver"||!known||!ready);if(item.Key!="Flydigi transport driver")item.Value.Text=known&&ready?"Installed":"Install";}
            string[] headlines={"Ready to bridge","Preparing bridge","Ready to play","Waiting for controller","Restoring controller","Profile recovery needed","Session needs attention"};status.Text=headlines[(int)state.Phase];description.Text=state.Message;
            if(running&&session.Installing)status.Text="Installing component";else if(running&&session.Mode=="Requirements")status.Text="Checking setup";else if(running&&session.Mode=="ValidateSetup")status.Text="Checking installers";
            pill.Text=state.Phase==BridgePhase.Active?"BRIDGE ACTIVE":state.Phase==BridgePhase.Waiting?"CONTROLLER OFFLINE":state.Phase==BridgePhase.Restoring?"RESTORING":pending?"RECOVERY NEEDED":running?"PREPARING":"BRIDGE OFF";pill.ForeColor=pending||state.Phase==BridgePhase.Failed?Theme.Amber:Theme.Accent;
            firmware.Text=state.Firmware=="Not connected"?"Firmware will appear after connection":"Firmware "+state.Firmware+"  /  Xbox One virtual controller";
            for(int i=0;i<4;i++){motors[i].Value=state.Motors[i];motors[i].Invalidate();}pad.Motors=(int[])state.Motors.Clone();pad.LT=state.LeftTrigger;pad.RT=state.RightTrigger;pad.Invalidate();leftInput.Value=state.LeftTrigger;rightInput.Value=state.RightTrigger;leftInput.Invalidate();rightInput.Invalidate();inputCount.Text="INPUT EVENTS\n"+state.InputChanges.ToString("N0");
            foreach(var pair in requirementLabels){bool value;if(state.Requirements.TryGetValue(pair.Key,out value)){pair.Value.Text=value?"READY":"NEEDS SETUP";pair.Value.ForeColor=value?Theme.Accent:Theme.Amber;}}
            if(activityCount!=session.ActivityRevision){activity.Text=string.Join(Environment.NewLine,session.Activity.ToArray());activity.SelectionStart=activity.TextLength;activity.ScrollToCaret();activityCount=session.ActivityRevision;}
            footer.Text=running?"Session "+(DateTime.Now-started).ToString(@"hh\:mm\:ss")+"  ·  "+(session.Mode=="Bridge"?"Runs until you stop it.":"Controller check in progress."):pending?"Recovery backup is retained. Keep Space Station closed.":"Bridge stopped. Start a session before opening your game.";
            if(tray!=null)tray.Text="Flydigi Trigger Bridge — "+pill.Text.ToLowerInvariant();
        }
        private void BuildTray(){var menu=new ContextMenuStrip();menu.Items.Add("Open Flydigi Trigger Bridge",null,(s,e)=>ShowWindow());menu.Items.Add("Start bridge",null,(s,e)=>{try{Begin("Bridge",0);}catch(Exception ex){ShowError(ex.Message);}});menu.Items.Add("Stop and restore",null,(s,e)=>session.RequestStop());menu.Items.Add(new ToolStripSeparator());menu.Items.Add("Exit",null,(s,e)=>Close());tray=new NotifyIcon{Icon=Icon,Text="Flydigi Trigger Bridge",Visible=true,ContextMenuStrip=menu};tray.DoubleClick+=(s,e)=>ShowWindow();}
        private void ShowWindow(){Show();WindowState=FormWindowState.Normal;Activate();}
        private void OnClosingSession(object sender,FormClosingEventArgs e){if(session.Running){e.Cancel=true;closeAfterCleanup=true;session.RequestStop();ShowWindow();}else{SaveSettings();if(timer!=null)timer.Stop();if(tray!=null){tray.Visible=false;tray.Dispose();}}}
        private void ShowError(string message){if(session!=null)session.Log(message);MessageBox.Show(this,message,"Flydigi Trigger Bridge",MessageBoxButtons.OK,MessageBoxIcon.Information);}
        private void OpenFolder(string path){Directory.CreateDirectory(path);Process.Start(new ProcessStartInfo("explorer.exe",CommandLine.Quote(path)){UseShellExecute=true});}
        private void ShowSetupGuide(){ShowDocument(Path.Combine("docs","SETUP.md"),"Setup guide");}
        private void ShowDocument(string relative,string caption){if(demo)return;string path=Path.Combine(session.RuntimeRoot,relative);string text=File.Exists(path)?File.ReadAllText(path):"Documentation is missing from the packaged runtime.";using(var dialog=new Form{Text=caption,ClientSize=new Size(820,640),StartPosition=FormStartPosition.CenterParent,BackColor=Theme.Back}){var view=new RichTextBox{Dock=DockStyle.Fill,Text=text,ReadOnly=true,BackColor=Theme.Panel,ForeColor=Theme.Text,Font=Theme.Font(11),BorderStyle=BorderStyle.None,DetectUrls=true};view.LinkClicked+=(s,e)=>Process.Start(new ProcessStartInfo(e.LinkText){UseShellExecute=true});dialog.Controls.Add(view);dialog.ShowDialog(this);}}
        private void LoadSettings(){try{string path=Path.Combine(session.DataRoot,"settings.txt");if(File.Exists(path)){string[] lines=File.ReadAllLines(path);minimize.Checked=Array.IndexOf(lines,"minimizeToTray=true")>=0;detailed.Checked=Array.IndexOf(lines,"diagnosticLogging=true")>=0;}}catch(Exception e){session.Log("Settings could not be read: "+e.Message);}}
        private void SaveSettings(){if(session==null||demo)return;try{File.WriteAllLines(Path.Combine(session.DataRoot,"settings.txt"),new[]{"minimizeToTray="+minimize.Checked.ToString().ToLowerInvariant(),"diagnosticLogging="+detailed.Checked.ToString().ToLowerInvariant()});}catch(Exception e){session.Log("Settings could not be saved: "+e.Message);}}
        private void ExportDiagnostics(){using(var dialog=new SaveFileDialog{Filter="ZIP archive|*.zip",FileName="FlydigiTriggerBridge-diagnostics.zip"}){if(dialog.ShowDialog(this)!=DialogResult.OK)return;using(var file=new FileStream(dialog.FileName,FileMode.Create))using(var zip=new ZipArchive(file,ZipArchiveMode.Create)){var entry=zip.CreateEntry("summary.txt");using(var writer=new StreamWriter(entry.Open())){writer.WriteLine("Flydigi Trigger Bridge "+Program.Version);writer.WriteLine("State: "+session.State.Phase);writer.WriteLine("Firmware: "+session.State.Firmware);foreach(var pair in session.State.Requirements)writer.WriteLine(pair.Key+": "+pair.Value);writer.WriteLine();foreach(string line in session.Activity)writer.WriteLine(line.Replace(session.DataRoot,"[app data]").Replace(Environment.UserName,"[user]"));}}session.Log("Diagnostic summary exported. Original profiles and device addresses were excluded.");}}
    }
}
