
#include <Arduino.h>
#include "CACerts.h"


const char * CACerts::getCert(const String& host)
{
  auto search = ca_certs.find(host);
  if (search != ca_certs.end())
  {
    return search->second;
  }
  else
  {
    return nullptr;
  }
}

// One-liners to pull root CA certs

/***************
 openssl s_client -showcerts -verify 5 -connect api.weather.gov:443 < /dev/null | awk '/BEGIN/,/END/{ \
 if(/BEGIN/){a++}; out="cert"a".pem"; print >out}'; for cert in *.pem; do newname=$(openssl x509 -noout -subject -in \
 $cert | sed -nE 's/.*CN ?= ?(.*)/\1/; s/[ ,.*]/_/g; s/__/_/g; s/_-_/-/; s/^_//g;p' | tr '[:upper:]' '[:lower:]').pem; \
 echo "${newname}"; mv "${cert}" "${newname}"; done
***************/

// clang-format off
//  Certificate:
//    Data:
//        Version: 3 (0x2)
//        Serial Number:
//            01:fd:a3:eb:6e:ca:75:c8:88:43:8b:72:4b:cf:bc:91
//        Signature Algorithm: sha256WithRSAEncryption
//        Issuer: C = US, O = DigiCert Inc, OU = www.digicert.com, CN = DigiCert Global Root CA
//        Validity
//            Not Before: Mar  8 12:00:00 2013 GMT
//            Not After : Mar  8 12:00:00 2023 GMT
//        Subject: C = US, O = DigiCert Inc, CN = DigiCert SHA2 Secure Server CA
//        Subject Public Key Info:
//            Public Key Algorithm: rsaEncryption
//                RSA Public-Key: (2048 bit)
//                Modulus:
//                    00:dc:ae:58:90:4d:c1:c4:30:15:90:35:5b:6e:3c:
//                    82:15:f5:2c:5c:bd:e3:db:ff:71:43:fa:64:25:80:
//                    d4:ee:18:a2:4d:f0:66:d0:0a:73:6e:11:98:36:17:
//                    64:af:37:9d:fd:fa:41:84:af:c7:af:8c:fe:1a:73:
//                    4d:cf:33:97:90:a2:96:87:53:83:2b:b9:a6:75:48:
//                    2d:1d:56:37:7b:da:31:32:1a:d7:ac:ab:06:f4:aa:
//                    5d:4b:b7:47:46:dd:2a:93:c3:90:2e:79:80:80:ef:
//                    13:04:6a:14:3b:b5:9b:92:be:c2:07:65:4e:fc:da:
//                    fc:ff:7a:ae:dc:5c:7e:55:31:0c:e8:39:07:a4:d7:
//                    be:2f:d3:0b:6a:d2:b1:df:5f:fe:57:74:53:3b:35:
//                    80:dd:ae:8e:44:98:b3:9f:0e:d3:da:e0:d7:f4:6b:
//                    29:ab:44:a7:4b:58:84:6d:92:4b:81:c3:da:73:8b:
//                    12:97:48:90:04:45:75:1a:dd:37:31:97:92:e8:cd:
//                    54:0d:3b:e4:c1:3f:39:5e:2e:b8:f3:5c:7e:10:8e:
//                    86:41:00:8d:45:66:47:b0:a1:65:ce:a0:aa:29:09:
//                    4e:f3:97:eb:e8:2e:ab:0f:72:a7:30:0e:fa:c7:f4:
//                    fd:14:77:c3:a4:5b:28:57:c2:b3:f9:82:fd:b7:45:
//                    58:9b
//                Exponent: 65537 (0x10001)
//        X509v3 extensions:
//            X509v3 Basic Constraints: critical
//                CA:TRUE, pathlen:0
//            X509v3 Key Usage: critical
//                Digital Signature, Certificate Sign, CRL Sign
//            Authority Information Access:
//                OCSP - URI:http://ocsp.digicert.com
//
//            X509v3 CRL Distribution Points:
//
//                Full Name:
//                  URI:http://crl3.digicert.com/DigiCertGlobalRootCA.crl
//
//                Full Name:
//                  URI:http://crl4.digicert.com/DigiCertGlobalRootCA.crl
//
//            X509v3 Certificate Policies:
//                Policy: X509v3 Any Policy
//                  CPS: https://www.digicert.com/CPS
//
//            X509v3 Subject Key Identifier:
//                0F:80:61:1C:82:31:61:D5:2F:28:E7:8D:46:38:B4:2C:E1:C6:D9:E2
//            X509v3 Authority Key Identifier:
//                keyid:03:DE:50:35:56:D1:4C:BB:66:F0:A3:E2:1B:1B:C3:97:B2:3D:D1:55
//
//    Signature Algorithm: sha256WithRSAEncryption
//         23:3e:df:4b:d2:31:42:a5:b6:7e:42:5c:1a:44:cc:69:d1:68:
//         b4:5d:4b:e0:04:21:6c:4b:e2:6d:cc:b1:e0:97:8f:a6:53:09:
//         cd:aa:2a:65:e5:39:4f:1e:83:a5:6e:5c:98:a2:24:26:e6:fb:
//         a1:ed:93:c7:2e:02:c6:4d:4a:bf:b0:42:df:78:da:b3:a8:f9:
//         6d:ff:21:85:53:36:60:4c:76:ce:ec:38:dc:d6:51:80:f0:c5:
//         d6:e5:d4:4d:27:64:ab:9b:c7:3e:71:fb:48:97:b8:33:6d:c9:
//         13:07:ee:96:a2:1b:18:15:f6:5c:4c:40:ed:b3:c2:ec:ff:71:
//         c1:e3:47:ff:d4:b9:00:b4:37:42:da:20:c9:ea:6e:8a:ee:14:
//         06:ae:7d:a2:59:98:88:a8:1b:6f:2d:f4:f2:c9:14:5f:26:cf:
//         2c:8d:7e:ed:37:c0:a9:d5:39:b9:82:bf:19:0c:ea:34:af:00:
//         21:68:f8:ad:73:e2:c9:32:da:38:25:0b:55:d3:9a:1d:f0:68:
//         86:ed:2e:41:34:ef:7c:a5:50:1d:bf:3a:f9:d3:c1:08:0c:e6:
//         ed:1e:8a:58:25:e4:b8:77:ad:2d:6e:f5:52:dd:b4:74:8f:ab:
//         49:2e:9d:3b:93:34:28:1f:78:ce:94:ea:c7:bd:d3:c9:6d:1c:
//         de:5c:32:f3
// clang-format on
//
namespace {
const char PROGMEM digicert_sha2_secure_server_ca[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIElDCCA3ygAwIBAgIQAf2j627KdciIQ4tyS8+8kTANBgkqhkiG9w0BAQsFADBh"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD"
    "QTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaME0xCzAJBgNVBAYTAlVT"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJzAlBgNVBAMTHkRpZ2lDZXJ0IFNIQTIg"
    "U2VjdXJlIFNlcnZlciBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB"
    "ANyuWJBNwcQwFZA1W248ghX1LFy949v/cUP6ZCWA1O4Yok3wZtAKc24RmDYXZK83"
    "nf36QYSvx6+M/hpzTc8zl5CilodTgyu5pnVILR1WN3vaMTIa16yrBvSqXUu3R0bd"
    "KpPDkC55gIDvEwRqFDu1m5K+wgdlTvza/P96rtxcflUxDOg5B6TXvi/TC2rSsd9f"
    "/ld0Uzs1gN2ujkSYs58O09rg1/RrKatEp0tYhG2SS4HD2nOLEpdIkARFdRrdNzGX"
    "kujNVA075ME/OV4uuPNcfhCOhkEAjUVmR7ChZc6gqikJTvOX6+guqw9ypzAO+sf0"
    "/RR3w6RbKFfCs/mC/bdFWJsCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8C"
    "AQAwDgYDVR0PAQH/BAQDAgGGMDQGCCsGAQUFBwEBBCgwJjAkBggrBgEFBQcwAYYY"
    "aHR0cDovL29jc3AuZGlnaWNlcnQuY29tMHsGA1UdHwR0MHIwN6A1oDOGMWh0dHA6"
    "Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RDQS5jcmwwN6A1"
    "oDOGMWh0dHA6Ly9jcmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbFJvb3RD"
    "QS5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEWHGh0dHBzOi8v"
    "d3d3LmRpZ2ljZXJ0LmNvbS9DUFMwHQYDVR0OBBYEFA+AYRyCMWHVLyjnjUY4tCzh"
    "xtniMB8GA1UdIwQYMBaAFAPeUDVW0Uy7ZvCj4hsbw5eyPdFVMA0GCSqGSIb3DQEB"
    "CwUAA4IBAQAjPt9L0jFCpbZ+QlwaRMxp0Wi0XUvgBCFsS+JtzLHgl4+mUwnNqipl"
    "5TlPHoOlblyYoiQm5vuh7ZPHLgLGTUq/sELfeNqzqPlt/yGFUzZgTHbO7Djc1lGA"
    "8MXW5dRNJ2Srm8c+cftIl7gzbckTB+6WohsYFfZcTEDts8Ls/3HB40f/1LkAtDdC"
    "2iDJ6m6K7hQGrn2iWZiIqBtvLfTyyRRfJs8sjX7tN8Cp1Tm5gr8ZDOo0rwAhaPit"
    "c+LJMto4JQtV05od8GiG7S5BNO98pVAdvzr508EIDObtHopYJeS4d60tbvVS3bR0"
    "j6tJLp07kzQoH3jOlOrHvdPJbRzeXDLz\n"
    "-----END CERTIFICATE-----\n";
}


const char PROGMEM digicert_tls_rsa_sha256_2020_ca1[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIEvjCCA6agAwIBAgIQBtjZBNVYQ0b2ii+nVCJ+xDANBgkqhkiG9w0BAQsFADBh"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3"
"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD"
"QTAeFw0yMTA0MTQwMDAwMDBaFw0zMTA0MTMyMzU5NTlaME8xCzAJBgNVBAYTAlVT"
"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxKTAnBgNVBAMTIERpZ2lDZXJ0IFRMUyBS"
"U0EgU0hBMjU2IDIwMjAgQ0ExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC"
"AQEAwUuzZUdwvN1PWNvsnO3DZuUfMRNUrUpmRh8sCuxkB+Uu3Ny5CiDt3+PE0J6a"
"qXodgojlEVbbHp9YwlHnLDQNLtKS4VbL8Xlfs7uHyiUDe5pSQWYQYE9XE0nw6Ddn"
"g9/n00tnTCJRpt8OmRDtV1F0JuJ9x8piLhMbfyOIJVNvwTRYAIuE//i+p1hJInuW"
"raKImxW8oHzf6VGo1bDtN+I2tIJLYrVJmuzHZ9bjPvXj1hJeRPG/cUJ9WIQDgLGB"
"Afr5yjK7tI4nhyfFK3TUqNaX3sNk+crOU6JWvHgXjkkDKa77SU+kFbnO8lwZV21r"
"eacroicgE7XQPUDTITAHk+qZ9QIDAQABo4IBgjCCAX4wEgYDVR0TAQH/BAgwBgEB"
"/wIBADAdBgNVHQ4EFgQUt2ui6qiqhIx56rTaD5iyxZV2ufQwHwYDVR0jBBgwFoAU"
"A95QNVbRTLtm8KPiGxvDl7I90VUwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQG"
"CCsGAQUFBwMBBggrBgEFBQcDAjB2BggrBgEFBQcBAQRqMGgwJAYIKwYBBQUHMAGG"
"GGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBABggrBgEFBQcwAoY0aHR0cDovL2Nh"
"Y2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNydDBCBgNV"
"HR8EOzA5MDegNaAzhjFodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRH"
"bG9iYWxSb290Q0EuY3JsMD0GA1UdIAQ2MDQwCwYJYIZIAYb9bAIBMAcGBWeBDAEB"
"MAgGBmeBDAECATAIBgZngQwBAgIwCAYGZ4EMAQIDMA0GCSqGSIb3DQEBCwUAA4IB"
"AQCAMs5eC91uWg0Kr+HWhMvAjvqFcO3aXbMM9yt1QP6FCvrzMXi3cEsaiVi6gL3z"
"ax3pfs8LulicWdSQ0/1s/dCYbbdxglvPbQtaCdB73sRD2Cqk3p5BJl+7j5nL3a7h"
"qG+fh/50tx8bIKuxT8b1Z11dmzzp/2n3YWzW2fP9NsarA4h20ksudYbj/NhVfSbC"
"EXffPgK2fPOre3qGNm+499iTcc+G33Mw+nur7SpZyEKEOxEXGlLzyQ4UfaJbcme6"
"ce1XR2bFuAJKZTRei9AqPCCcUZlM51Ke92sRKw2Sfh3oius2FkOH6ipjv3U/697E"
"A7sKPPcw7+uvTPyLNhBzPvOk\n"
"-----END CERTIFICATE-----\n";

const std::map<String, const char *> CACerts::ca_certs{
  {"airquality.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"alerts-v2.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"alerts.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"amdar.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"api-v1.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"api.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"aviationweather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"cfs.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"digital.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"f1.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"forecast-v3.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"forecast.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"gisc-washington.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"graphical.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"hads.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"hysplit.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"idpgis.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"inws.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"iris.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"madis-data.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"madis.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"mag.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"magpara.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"marine.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"mobile.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"new.nowcoast.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"nomads.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"nomads.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"nowcoast.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"nowcoast.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"nws.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"opengeo.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview-alerts.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview-api.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview-forecast-v3.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview-opengeo.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview-radar.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"preview.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"products.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"ptwc.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"ra4-gifs.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"radar2pub.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"radar3pub.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"radar-v2.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"radar.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"ripcurrents.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"rsmc.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"ssd.wrh.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"test-nomads.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"tsunami.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"w1.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"w2.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"water.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.aviationweather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.cpcpara.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.lib.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.nco.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.ndsc.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.nowcoast.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.nws.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.opah.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.prh.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.ripcurrents.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.srh.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.tsunami.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.tsunami.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.vwt.ncep.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"www.wrh.noaa.gov", digicert_tls_rsa_sha256_2020_ca1},
  {"wwwx.wrh.noaa.gov", digicert_tls_rsa_sha256_2020_ca1}
};

std::map<String, const char *> ca_certs{
  {"api.weather.gov", digicert_tls_rsa_sha256_2020_ca1},
};
