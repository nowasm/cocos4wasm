# dbglisten.ps1 — capture OutputDebugString (DBWIN protocol) to a file.
# Usage: powershell -File dbglisten.ps1 -OutFile out.txt -Seconds 15
param(
    [string]$OutFile = "dbg.txt",
    [int]$Seconds = 15
)

Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Threading;
using System.IO.MemoryMappedFiles;

public static class DbgListener {
    public static void Run(string outFile, int seconds) {
        using (var bufferReady = new EventWaitHandle(false, EventResetMode.AutoReset, "DBWIN_BUFFER_READY"))
        using (var dataReady   = new EventWaitHandle(false, EventResetMode.AutoReset, "DBWIN_DATA_READY"))
        using (var mmf = MemoryMappedFile.CreateOrOpen("DBWIN_BUFFER", 4096))
        using (var accessor = mmf.CreateViewAccessor())
        using (var sw = new StreamWriter(outFile, false, Encoding.UTF8)) {
            var end = DateTime.UtcNow.AddSeconds(seconds);
            bufferReady.Set();
            var bytes = new byte[4096 - 4];
            while (DateTime.UtcNow < end) {
                if (!dataReady.WaitOne(500)) continue;
                int pid = accessor.ReadInt32(0);
                accessor.ReadArray(4, bytes, 0, bytes.Length);
                int len = Array.IndexOf(bytes, (byte)0);
                if (len < 0) len = bytes.Length;
                string msg = Encoding.UTF8.GetString(bytes, 0, len);
                sw.Write("[" + pid + "] " + msg);
                sw.Flush();
                bufferReady.Set();
            }
        }
    }
}
"@

[DbgListener]::Run($OutFile, $Seconds)
