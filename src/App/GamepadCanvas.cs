using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace FlydigiTriggerBridge {
    internal sealed class GamepadCanvas:Control {
        internal int[] Motors=new int[4];
        internal int LT,RT;
        internal GamepadCanvas(){DoubleBuffered=true;BackColor=Theme.Panel;}
        internal static GraphicsPath Shell(){
            var p=new GraphicsPath();
            p.AddBezier(119,58,172,51,308,51,361,58);
            p.AddBezier(361,58,394,63,407,81,418,114);
            p.AddBezier(418,114,432,157,461,245,427,273);
            p.AddBezier(427,273,405,291,380,277,353,237);
            p.AddBezier(353,237,340,216,328,207,312,207);
            p.AddLine(312,207,168,207);
            p.AddBezier(168,207,152,207,140,216,127,237);
            p.AddBezier(127,237,100,277,75,291,53,273);
            p.AddBezier(53,273,19,245,48,157,62,114);
            p.AddBezier(62,114,73,81,86,63,119,58);
            p.CloseFigure();return p;
        }
        private static void Disc(Graphics g,float x,float y,float radius,Color color){using(var b=new SolidBrush(color))g.FillEllipse(b,x-radius,y-radius,radius*2,radius*2);}
        private static void Stick(Graphics g,float x,float y){
            Disc(g,x,y,28,Color.FromArgb(12,18,26));
            using(var pen=new Pen(Color.FromArgb(85,103,124),1.6f))g.DrawEllipse(pen,x-27,y-27,54,54);
            using(var b=new LinearGradientBrush(new RectangleF(x-21,y-21,42,42),Color.FromArgb(60,74,91),Color.FromArgb(24,34,47),90))g.FillEllipse(b,x-21,y-21,42,42);
            using(var pen=new Pen(Color.FromArgb(118,137,157),1))g.DrawEllipse(pen,x-21,y-21,42,42);
            using(var pen=new Pen(Color.FromArgb(29,42,57),1))g.DrawEllipse(pen,x-15,y-15,30,30);
        }
        private static void TextAt(Graphics g,string text,RectangleF box,Color color,float size){using(var f=Theme.Font(size,FontStyle.Bold))using(var b=new SolidBrush(color))using(var format=new StringFormat{Alignment=StringAlignment.Center,LineAlignment=StringAlignment.Center})g.DrawString(text,f,b,box,format);}
        protected override void OnPaint(PaintEventArgs e){
            var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            float scale=Math.Min(Width/480f,Height/300f);
            g.TranslateTransform((Width-480*scale)/2,(Height-300*scale)/2);g.ScaleTransform(scale,scale);
            using(var shell=Shell()){
                using(var b=new LinearGradientBrush(new Rectangle(38,54,404,229),Color.FromArgb(48,63,81),Color.FromArgb(20,29,41),90))g.FillPath(b,shell);
                var saved=g.Save();g.SetClip(shell,CombineMode.Intersect);
                // Every face detail and motor highlight shares the shell clip.
                // Trigger panels are inset into the shoulders, never detached.
                for(int side=0;side<2;side++){
                    float x=side==0?109:282;
                    var trigger=new RectangleF(x,74,89,25);
                    Theme.Box(g,trigger,Color.FromArgb(14,22,32),7);
                    using(var p=Theme.Rounded(new RectangleF(x+.5f,74.5f,88,24),7))using(var pen=new Pen(Motors[2+side]>0?Theme.Accent:Color.FromArgb(83,102,121),1.2f))g.DrawPath(pen,p);
                    TextAt(g,side==0?"LT":"RT",trigger,Motors[2+side]>0?Theme.Accent:Theme.Muted,8);
                    if(Motors[2+side]>0)Theme.Box(g,new RectangleF(x+8,93,73*Math.Min(100,Motors[2+side])/100f,2),Theme.Accent,1);
                    using(var grip=new GraphicsPath()){
                        grip.AddBezier(57,198,70,183,89,187,118,221);
                        grip.AddBezier(118,221,103,249,86,272,66,265);
                        grip.AddBezier(66,265,51,258,48,231,57,198);grip.CloseFigure();
                        if(side==1){using(var mirror=new Matrix(-1,0,0,1,480,0))grip.Transform(mirror);}
                        using(var b=new SolidBrush(Motors[side]>0?Color.FromArgb(37,78,85):Color.FromArgb(24,35,48)))g.FillPath(b,grip);
                        using(var pen=new Pen(Motors[side]>0?Color.FromArgb(83,180,183):Color.FromArgb(47,65,84),1))g.DrawPath(pen,grip);
                    }
                    using(var pen=new Pen(Color.FromArgb(45,63,81),1)){for(int i=0;i<4;i++){float gx=side==0?64+i*6:416-i*6;g.DrawLine(pen,gx,217,gx+(side==0?8:-8),233);}}
                }
                Stick(g,136,133);Stick(g,299,175);
                Disc(g,184,175,25,Color.FromArgb(13,21,30));
                PointF[] cross={new PointF(177,155),new PointF(191,155),new PointF(191,168),new PointF(204,168),new PointF(204,182),new PointF(191,182),new PointF(191,195),new PointF(177,195),new PointF(177,182),new PointF(164,182),new PointF(164,168),new PointF(177,168)};
                using(var b=new LinearGradientBrush(new Rectangle(164,155,40,40),Color.FromArgb(125,144,163),Color.FromArgb(62,80,100),90))g.FillPolygon(b,cross);
                Disc(g,240,122,13,Color.FromArgb(17,26,38));
                using(var pen=new Pen(Color.FromArgb(138,159,181),1.5f)){g.DrawArc(pen,234,116,12,12,-55,290);g.DrawLine(pen,240,114,240,122);}
                for(int i=0;i<2;i++){float x=i==0?218:262;Disc(g,x,157,9,Color.FromArgb(16,25,36));using(var pen=new Pen(Theme.Muted,1)){g.DrawLine(pen,x-3,155,x+3,155);g.DrawLine(pen,x-3,158,x+3,158);}}
                string[] keys={"Y","B","A","X"};Point[] dots={new Point(350,113),new Point(371,134),new Point(350,155),new Point(329,134)};
                Color[] colors={Color.FromArgb(217,202,131),Color.FromArgb(214,144,151),Color.FromArgb(154,205,169),Color.FromArgb(145,191,228)};
                for(int i=0;i<4;i++){Disc(g,dots[i].X,dots[i].Y,12,Color.FromArgb(11,19,28));using(var pen=new Pen(Color.FromArgb(79,98,118),1))g.DrawEllipse(pen,dots[i].X-12,dots[i].Y-12,24,24);TextAt(g,keys[i],new RectangleF(dots[i].X-12,dots[i].Y-12,24,24),colors[i],8);}
                g.Restore(saved);using(var pen=new Pen(Color.FromArgb(89,109,131),1.6f))g.DrawPath(pen,shell);
            }
        }
    }
}
