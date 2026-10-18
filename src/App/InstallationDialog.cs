using System;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace FlydigiTriggerBridge {
    internal static class InstallationDialog {
        internal static bool Confirm(Form owner,string component,string runtime){
            string heading,description,license,consent;
            switch(component){
                case "WinUHid":
                    heading="Install virtual controller driver";
                    description="Installs the bundled WinUHid user-mode driver and trusts the Flydigi Bridge Driver public signing certificate in this PC's Trusted Root and Trusted Publisher stores. These changes persist until removed. An existing working WinUHid is kept. The official Visual C++ installer opens first if its x64 runtime is missing. Windows may request a restart.";
                    license=Path.Combine(runtime,"drivers","WinUHid","LICENSE");consent="I authorize this driver installation and certificate trust.";break;
                case "HidHide":
                    heading="Install HidHide";
                    description="Installs the bundled official Nefarius HidHide filter. This changes the Windows HID device stack and persists after the app closes. It does not enable hiding or change device rules. Reconnect the dongle or restart Windows when requested. An existing installation is kept.";
                    license=Path.Combine(runtime,"drivers","HidHide","LICENSE");consent="I accept the license and authorize installing the HidHide filter.";break;
                case "GameInput":
                    heading="Install Microsoft GameInput";
                    description="Opens the bundled official Microsoft GameInput installer. The redistributable is licensed separately under the Microsoft terms below. An existing runtime is kept. Complete the installer and then run Check setup.";
                    license=Path.Combine(runtime,"drivers","GameInput","LICENSE.txt");consent="I accept these Microsoft license terms and authorize installation.";break;
                case "RemoveWinUHid":
                    heading="Remove this app's virtual driver";
                    description="Removes only the WinUHid device, driver package and certificate trust recorded by this application. Other software using this virtual driver will also lose it. An unrelated or changed driver is refused. The Flydigi transport driver, HidHide and GameInput are kept.";
                    license=Path.Combine(runtime,"drivers","WinUHid","LICENSE");consent="I authorize removing this application's recorded virtual driver.";break;
                default:throw new ArgumentException("Unknown installation component");
            }
            using(var dialog=new Form{Text="Flydigi Trigger Bridge setup",ClientSize=new Size(730,650),MinimumSize=new Size(690,620),StartPosition=FormStartPosition.CenterParent,BackColor=Theme.Back,ForeColor=Theme.Text,Font=Theme.Font(10)}){
                var grid=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(24),ColumnCount=1,RowCount=5};
                grid.RowStyles.Add(new RowStyle(SizeType.Absolute,47));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,133));grid.RowStyles.Add(new RowStyle(SizeType.Percent,100));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,58));grid.RowStyles.Add(new RowStyle(SizeType.Absolute,49));dialog.Controls.Add(grid);
                grid.Controls.Add(new Label{Text=heading,Font=Theme.Font(18,FontStyle.Bold),ForeColor=Theme.Text,Dock=DockStyle.Fill},0,0);
                grid.Controls.Add(new Label{Text=description,ForeColor=Theme.Muted,Dock=DockStyle.Fill},0,1);
                grid.Controls.Add(new RichTextBox{Text=File.ReadAllText(license),ReadOnly=true,Dock=DockStyle.Fill,BackColor=Theme.Panel,ForeColor=Theme.Muted,Font=Theme.Font(9),BorderStyle=BorderStyle.None},0,2);
                var accept=new CheckBox{Text=consent,Dock=DockStyle.Fill,ForeColor=Theme.Text};grid.Controls.Add(accept,0,3);
                var actions=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft};grid.Controls.Add(actions,0,4);
                var cancel=new ActionButton{Text="Cancel",Width=115,DialogResult=DialogResult.Cancel};
                var install=new ActionButton{Text=component=="RemoveWinUHid"?"Remove driver":"Install",Width=135,Primary=true,Enabled=false,DialogResult=DialogResult.OK};accept.CheckedChanged+=(s,e)=>install.Enabled=accept.Checked;
                actions.Controls.Add(cancel);actions.Controls.Add(install);dialog.CancelButton=cancel;
                return dialog.ShowDialog(owner)==DialogResult.OK;
            }
        }
    }
}
