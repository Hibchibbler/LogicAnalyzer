using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace GizyitClient
{
    public partial class BitSelector : UserControl
    {
        public BitSelector()
        {
            InitializeComponent();
        }

        public char[] RawData
        {
            get
            {
                char[] b = new char[4];
                b[0] = b[1]= b[2] =b[3] ='\0';

                b[0]  = (char)(int.Parse(lblBit0.Text) << 0);
                b[0] |= (char)(int.Parse(lblBit1.Text) << 1);
                b[0] |= (char)(int.Parse(lblBit2.Text) << 2);
                b[0] |= (char)(int.Parse(lblBit3.Text) << 3);
                b[0] |= (char)(int.Parse(lblBit4.Text) << 4);
                b[0] |= (char)(int.Parse(lblBit5.Text) << 5);
                b[0] |= (char)(int.Parse(lblBit6.Text) << 6);
                b[0] |= (char)(int.Parse(lblBit7.Text) << 7);

                b[1]  = (char)(int.Parse(lblBit8.Text)  << 0);
                b[1] |= (char)(int.Parse(lblBit9.Text)  << 1);
                b[1] |= (char)(int.Parse(lblBit10.Text) << 2);
                b[1] |= (char)(int.Parse(lblBit11.Text) << 3);
                b[1] |= (char)(int.Parse(lblBit12.Text) << 4);
                b[1] |= (char)(int.Parse(lblBit13.Text) << 5);
                b[1] |= (char)(int.Parse(lblBit14.Text) << 6);
                b[1] |= (char)(int.Parse(lblBit15.Text) << 7);

                b[2]  = (char)(int.Parse(lblBit16.Text) << 0);
                b[2] |= (char)(int.Parse(lblBit17.Text) << 1);
                b[2] |= (char)(int.Parse(lblBit18.Text) << 2);
                b[2] |= (char)(int.Parse(lblBit19.Text) << 3);
                b[2] |= (char)(int.Parse(lblBit20.Text) << 4);
                b[2] |= (char)(int.Parse(lblBit21.Text) << 5);
                b[2] |= (char)(int.Parse(lblBit22.Text) << 6);
                b[2] |= (char)(int.Parse(lblBit23.Text) << 7);

                b[3]  = (char)(int.Parse(lblBit24.Text) << 0);
                b[3] |= (char)(int.Parse(lblBit25.Text) << 1);
                b[3] |= (char)(int.Parse(lblBit26.Text) << 2);
                b[3] |= (char)(int.Parse(lblBit27.Text) << 3);
                b[3] |= (char)(int.Parse(lblBit28.Text) << 4);
                b[3] |= (char)(int.Parse(lblBit29.Text) << 5);
                b[3] |= (char)(int.Parse(lblBit30.Text) << 6);
                b[3] |= (char)(int.Parse(lblBit31.Text) << 7);
                return (b);
            }
        }

        private void chbxBit0_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit0.Checked)
                lblBit0.Text = "1";
            else
                lblBit0.Text = "0";
        }

        private void chbxBit1_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit1.Checked)
                lblBit1.Text = "1";
            else
                lblBit1.Text = "0";
        }

        private void chbxBit2_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit2.Checked)
                lblBit2.Text = "1";
            else
                lblBit2.Text = "0";
        }

        private void chbxBit3_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit3.Checked)
                lblBit3.Text = "1";
            else
                lblBit3.Text = "0";
        }

        private void chbxBit4_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit4.Checked)
                lblBit4.Text = "1";
            else
                lblBit4.Text = "0";
        }

        private void chbxBit5_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit5.Checked)
                lblBit5.Text = "1";
            else
                lblBit5.Text = "0";
        }

        private void chbxBit6_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit6.Checked)
                lblBit6.Text = "1";
            else
                lblBit6.Text = "0";
        }

        private void chbxBit7_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit7.Checked)
                lblBit7.Text = "1";
            else
                lblBit7.Text = "0";
        }


        private void chbxBit8_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit8.Checked)
                lblBit8.Text = "1";
            else
                lblBit8.Text = "0";
        }

        private void chbxBit9_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit9.Checked)
                lblBit9.Text = "1";
            else
                lblBit9.Text = "0";
        }

        private void chbxBit10_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit10.Checked)
                lblBit10.Text = "1";
            else
                lblBit10.Text = "0";
        }

        private void chbxBit11_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit11.Checked)
                lblBit11.Text = "1";
            else
                lblBit11.Text = "0";
        }

        private void chbxBit12_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit12.Checked)
                lblBit12.Text = "1";
            else
                lblBit12.Text = "0";
        }

        private void chbxBit13_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit13.Checked)
                lblBit13.Text = "1";
            else
                lblBit13.Text = "0";
        }

        private void chbxBit14_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit14.Checked)
                lblBit14.Text = "1";
            else
                lblBit14.Text = "0";
        }

        private void chbxBit15_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit15.Checked)
                lblBit15.Text = "1";
            else
                lblBit15.Text = "0";
        }

        private void chbxBit16_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit16.Checked)
                lblBit16.Text = "1";
            else
                lblBit16.Text = "0";
        }

        private void chbxBit17_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit17.Checked)
                lblBit17.Text = "1";
            else
                lblBit17.Text = "0";
        }

        private void chbxBit18_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit18.Checked)
                lblBit18.Text = "1";
            else
                lblBit18.Text = "0";
        }

        private void chbxBit19_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit19.Checked)
                lblBit19.Text = "1";
            else
                lblBit19.Text = "0";
        }

        private void chbxBit20_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit20.Checked)
                lblBit20.Text = "1";
            else
                lblBit20.Text = "0";
        }

        private void chbxBit21_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit21.Checked)
                lblBit21.Text = "1";
            else
                lblBit21.Text = "0";
        }

        private void chbxBit22_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit22.Checked)
                lblBit22.Text = "1";
            else
                lblBit22.Text = "0";
        }

        private void chbxBit23_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit23.Checked)
                lblBit23.Text = "1";
            else
                lblBit23.Text = "0";
        }

        private void chbxBit24_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit24.Checked)
                lblBit24.Text = "1";
            else
                lblBit24.Text = "0";
        }

        private void chbxBit25_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit25.Checked)
                lblBit25.Text = "1";
            else
                lblBit25.Text = "0";
        }

        private void chbxBit26_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit26.Checked)
                lblBit26.Text = "1";
            else
                lblBit26.Text = "0";
        }

        private void chbxBit27_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit27.Checked)
                lblBit27.Text = "1";
            else
                lblBit27.Text = "0";
        }
        private void chbxBit28_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit28.Checked)
                lblBit28.Text = "1";
            else
                lblBit28.Text = "0";
        }

        private void chbxBit29_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit29.Checked)
                lblBit29.Text = "1";
            else
                lblBit29.Text = "0";
        }

        private void chbxBit30_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit30.Checked)
                lblBit30.Text = "1";
            else
                lblBit30.Text = "0";
        }

        private void chbxBit31_CheckedChanged(object sender, EventArgs e)
        {
            if (chbxBit31.Checked)
                lblBit31.Text = "1";
            else
                lblBit31.Text = "0";
        }



    }
}
