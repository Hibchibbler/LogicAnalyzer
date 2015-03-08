using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;

namespace GizyitClient
{
    public partial class frmMain : Form
    {
        private System.Collections.Generic.List<char[]> middleBuffer = new System.Collections.Generic.List<char[]>();
        private System.Threading.Mutex mMutex = new System.Threading.Mutex(false);
        public frmMain()
        {
            InitializeComponent();
        }

   

        private void frmMain_Load(object sender, EventArgs e)
        {

        }

        

        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (btnConnect.Text.StartsWith("Connect"))
            {
                serialPort.PortName = "COM" + txtCOMPortNumber.Text;
                serialPort.BaudRate = int.Parse(txtBaudrate.Text);
                serialPort.Open();

                worker.RunWorkerAsync();
                btnConnect.Text = "Disconnect";
            }
            else
            {
                worker.CancelAsync();
                btnConnect.Text = "Connect";
            }
        }
        
        private char[] ReadSerialPort(SerialPort port)
        {
            char[] inBuffer = new char[256];
            lock (port)
            {
                port.Read(inBuffer, 0, 256);
            }
            return inBuffer;
        }

        private void WriteSerialPort(SerialPort port, char[] text)
        {
            lock (port)
            {
                port.Write(text,0,4);
            }
        }

        private void backgroundWorker1_DoWork(object sender, DoWorkEventArgs e)
        {
            int pc = 0;
            while (true)
            {
                if (worker.CancellationPending)
                {
                    e.Cancel = true;
                    break;
                }

                //char[] inBuffer = new char[256];

                char[] chunk = ReadSerialPort(serialPort);
                mMutex.WaitOne();
                    middleBuffer.Add(chunk);
                mMutex.ReleaseMutex();

                worker.ReportProgress(pc);
                pc = (pc + 1) % 100;
                System.Threading.Thread.Sleep(100);

            }
        }

        private void backgroundWorker1_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            mMutex.WaitOne();
                while (middleBuffer.Count > 0)
                {
                    txtDebug.Text += new String(middleBuffer.First<char[]>());
                    txtDebug.SelectAll();
                    txtDebug.ScrollToCaret();
                    middleBuffer.RemoveAt(0);                
                }
            mMutex.ReleaseMutex();
        }

        private void btnGeneric_Click(object sender, EventArgs e)
        {
            WriteSerialPort(serialPort, bitSelector1.RawData);
        }

        private void chbxBit_Click(object sender, EventArgs e)
        {

        }

        private void btnStart_Click(object sender, EventArgs e)
        {

        }
    }
}
