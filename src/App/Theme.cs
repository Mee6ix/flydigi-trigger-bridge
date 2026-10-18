using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace FlydigiTriggerBridge {
    internal static class Theme {
        internal static readonly Color Back=Color.FromArgb(13,18,24),Panel=Color.FromArgb(22,29,38),Border=Color.FromArgb(43,53,66),Text=Color.FromArgb(235,241,246),Muted=Color.FromArgb(142,157,173),Accent=Color.FromArgb(181,239,117),Cyan=Color.FromArgb(101,216,212),Amber=Color.FromArgb(244,193,106);
        internal static Font Font(float size,FontStyle style=FontStyle.Regular){return new Font("Segoe UI",size,style,GraphicsUnit.Point);}
        internal static GraphicsPath Rounded(RectangleF r,float radius){var p=new GraphicsPath();float d=radius*2;p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.Right-d,r.Y,d,d,270,90);p.AddArc(r.Right-d,r.Bottom-d,d,d,0,90);p.AddArc(r.X,r.Bottom-d,d,d,90,90);p.CloseFigure();return p;}
        internal static void Box(Graphics g,RectangleF r,Color fill,float radius){using(var p=Rounded(r,radius))using(var b=new SolidBrush(fill))g.FillPath(b,p);}
    }
    internal sealed class Card:Panel {
        internal Card(){DoubleBuffered=true;BackColor=Theme.Back;Padding=new Padding(24);}
        protected override void OnPaint(PaintEventArgs e){e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;Theme.Box(e.Graphics,new RectangleF(0,0,Width-1,Height-1),Theme.Panel,14);using(var p=Theme.Rounded(new RectangleF(.5f,.5f,Width-1,Height-1),14))using(var pen=new Pen(Theme.Border))e.Graphics.DrawPath(pen,p);base.OnPaint(e);}
    }
    internal sealed class ActionButton:Button {
        internal bool Primary;private bool over;
        internal ActionButton(){FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;Cursor=Cursors.Hand;Font=Theme.Font(10,FontStyle.Bold);Height=44;BackColor=Theme.Back;DoubleBuffered=true;}
        protected override void OnMouseEnter(EventArgs e){over=true;Invalidate();base.OnMouseEnter(e);}
        protected override void OnMouseLeave(EventArgs e){over=false;Invalidate();base.OnMouseLeave(e);}
        protected override void OnPaint(PaintEventArgs e){e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;Color fill=Primary?Theme.Accent:Theme.Panel;if(over&&Enabled)fill=Primary?Color.FromArgb(203,250,151):Color.FromArgb(36,46,59);if(!Enabled)fill=Color.FromArgb(33,42,52);Theme.Box(e.Graphics,new RectangleF(0,0,Width-1,Height-1),fill,9);if(!Primary)using(var outline=Theme.Rounded(new RectangleF(.5f,.5f,Width-2,Height-2),9))using(var pen=new Pen(Theme.Border))e.Graphics.DrawPath(pen,outline);TextRenderer.DrawText(e.Graphics,Text,Font,ClientRectangle,!Enabled?Theme.Muted:Primary?Theme.Back:Theme.Text,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPrefix);if(Focused)using(var pen=new Pen(Theme.Cyan))e.Graphics.DrawRectangle(pen,3,3,Width-7,Height-7);}
    }
    internal sealed class Meter:Control {
        internal int Value;internal Color Accent=Theme.Cyan;internal string Title="",Suffix="%";
        internal Meter(){DoubleBuffered=true;Height=67;BackColor=Theme.Panel;}
        protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;using(var f=Theme.Font(9))using(var b=new SolidBrush(Theme.Muted))g.DrawString(Title,f,b,0,0);using(var f=Theme.Font(15,FontStyle.Bold))using(var b=new SolidBrush(Theme.Text)){string text=Value+Suffix;var size=g.MeasureString(text,f);g.DrawString(text,f,b,Width-size.Width,0);}Theme.Box(g,new RectangleF(0,43,Width,7),Theme.Border,3);if(Value>0)Theme.Box(g,new RectangleF(0,43,Math.Max(7,Width*Math.Min(100,Value)/100f),7),Accent,3);}
    }
}
