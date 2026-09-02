$code = @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class CredMan {
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
  public struct CREDENTIAL {
    public int Flags; public int Type; public string TargetName; public string Comment;
    public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
    public int CredentialBlobSize; public IntPtr CredentialBlob; public int Persist;
    public int AttributeCount; public IntPtr Attributes; public string TargetAlias; public string UserName;
  }
  [DllImport("advapi32.dll", CharSet=CharSet.Unicode, EntryPoint="CredReadW")]
  public static extern bool CredRead(string target, int type, int flags, out IntPtr credPtr);
  public static string Read(string target) {
    IntPtr p;
    if (!CredRead(target, 1, 0, out p)) return null;
    CREDENTIAL c = (CREDENTIAL)Marshal.PtrToStructure(p, typeof(CREDENTIAL));
    byte[] blob = new byte[c.CredentialBlobSize];
    Marshal.Copy(c.CredentialBlob, blob, 0, blob.Length);
    string utf8 = Encoding.UTF8.GetString(blob);
    if (utf8.StartsWith("gh") || utf8.StartsWith("github_pat_")) return utf8;
    return Encoding.Unicode.GetString(blob);
  }
}
'@
Add-Type -TypeDefinition $code
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$t = [CredMan]::Read("GitHub - https://api.github.com/madtwo")
if ($t) { Write-Output ("LEN=" + $t.Length); Write-Output $t } else { Write-Output "NO_TOKEN" }
