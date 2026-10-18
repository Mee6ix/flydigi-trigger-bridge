using System;
using System.IO;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

// An offline, single-build key. It is never added to a Windows certificate store.
// The caller deletes the temporary PFX after signing; only the public CER ships.
internal static class NewDriverCertificate {
    private static int Main(string[] args){
        if(args.Length!=3)return 2;
        using(var rsa=new RSACng(3072)){
            var request=new CertificateRequest("CN=Flydigi Bridge Driver",rsa,HashAlgorithmName.SHA256,RSASignaturePadding.Pkcs1);
            request.CertificateExtensions.Add(new X509BasicConstraintsExtension(false,false,0,true));
            request.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature,true));
            var eku=new OidCollection();eku.Add(new Oid("1.3.6.1.5.5.7.3.3"));request.CertificateExtensions.Add(new X509EnhancedKeyUsageExtension(eku,true));
            using(var cert=request.CreateSelfSigned(DateTimeOffset.UtcNow.AddDays(-1),DateTimeOffset.UtcNow.AddYears(3))){
                string password=File.ReadAllText(args[2]);File.WriteAllBytes(args[0],cert.Export(X509ContentType.Pfx,password));File.WriteAllBytes(args[1],cert.Export(X509ContentType.Cert));
            }
        }
        return 0;
    }
}
