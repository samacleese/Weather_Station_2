
#include "CACerts.h"

#include <Arduino.h>  // for PROGMEM
#include <WString.h>

const char *CACerts::getCert(const String &host) {
    auto search = ca_certs.find(host);
    if (search != ca_certs.end()) {
        return search->second;
    } else {
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
//  # Pulled 5/10/2025
//  Certificate:
//      Data:
//          Version: 3 (0x2)
//          Serial Number:
//              06:fd:61:2f:3e:f1:40:8b:e0:9b:03:c7:1b:b0:06:71:e6:bd
//          Signature Algorithm: sha256WithRSAEncryption
//          Issuer: C = US, O = Let's Encrypt, CN = R11
//          Validity
//              Not Before: Apr 16 19:19:17 2025 GMT
//              Not After : Jul 15 19:19:16 2025 GMT
//          Subject: CN = weather.gov
//          Subject Public Key Info:
//              Public Key Algorithm: rsaEncryption
//                  Public-Key: (2048 bit)
//                  Modulus:
//                      00:c3:5a:88:19:d1:b4:fe:be:8a:a2:15:fd:b7:b6:
//                      81:a4:18:85:a7:b3:70:72:44:b8:20:9b:dc:82:56:
//                      de:89:c1:62:59:39:e9:3a:83:45:e2:90:f6:21:62:
//                      7f:d1:6c:68:3a:ba:ba:bb:1b:3d:aa:bf:dd:3d:06:
//                      aa:b4:1b:4a:99:6d:17:01:54:58:ca:b0:0c:6c:d7:
//                      12:2a:84:10:66:ce:9c:6d:a7:42:ca:a5:6d:be:0f:
//                      56:03:22:43:8d:4f:07:80:78:61:98:26:1c:4d:4d:
//                      a7:69:49:01:2e:38:05:4a:fc:0a:82:d8:99:58:9e:
//                      c3:6e:97:d9:99:06:20:25:dd:16:3e:3c:f0:ae:32:
//                      4b:f7:92:8a:2a:76:68:e8:e2:99:35:1b:22:33:d6:
//                      b2:9f:c5:29:19:6c:07:7e:26:97:98:73:f9:26:6b:
//                      df:41:20:49:de:0f:3b:d3:a1:9f:88:ee:cb:2c:65:
//                      79:fe:54:1d:66:53:1b:a3:44:b9:ff:ec:c1:b3:ce:
//                      0c:34:0d:16:a2:ff:00:fe:bd:a2:02:7a:d0:f1:38:
//                      c7:01:11:72:32:ea:56:eb:3f:be:86:b7:b7:a8:1f:
//                      6b:08:9f:14:17:28:02:56:6b:45:78:8c:f0:51:12:
//                      42:a5:59:b8:60:60:fd:65:21:e9:5f:8b:06:5f:96:
//                      c2:83
//                  Exponent: 65537 (0x10001)
//          X509v3 extensions:
//              X509v3 Key Usage: critical
//                  Digital Signature, Key Encipherment
//              X509v3 Extended Key Usage:
//                  TLS Web Server Authentication, TLS Web Client Authentication
//              X509v3 Basic Constraints: critical
//                  CA:FALSE
//              X509v3 Subject Key Identifier:
//                  04:60:F8:D8:E9:B2:3E:2D:E4:C2:74:D5:61:30:51:48:B6:20:5E:9E
//              X509v3 Authority Key Identifier:
//                  C5:CF:46:A4:EA:F4:C3:C0:7A:6C:95:C4:2D:B0:5E:92:2F:26:E3:B9
//              Authority Information Access:
//                  OCSP - URI:http://r11.o.lencr.org
//                  CA Issuers - URI:http://r11.i.lencr.org/
//              X509v3 Subject Alternative Name:
//                  DNS:alerts-v2.weather.gov, DNS:alerts.weather.gov, DNS:amdar.ncep.noaa.gov, DNS:amdar.noaa.gov, DNS:amdarqa.ncep.noaa.gov, DNS:api.weather.gov, DNS:forecast.weather.gov, DNS:hads.ncep.noaa.gov, DNS:hadsqa.ncep.noaa.gov, DNS:inws.ncep.noaa.gov, DNS:inwsqa.ncep.noaa.gov, DNS:iris.ncep.noaa.gov, DNS:irisqa.ncep.noaa.gov, DNS:madis.ncep.noaa.gov, DNS:madis.noaa.gov, DNS:madisqa.ncep.noaa.gov, DNS:marine.weather.gov, DNS:natwc.arh.noaa.gov, DNS:opengeo.ncep.noaa.gov, DNS:preview-alerts.weather.gov, DNS:preview-api.weather.gov, DNS:preview-forecast.weather.gov, DNS:preview-idp.weather.gov, DNS:preview-marine.weather.gov, DNS:preview-opengeo.ncep.noaa.gov, DNS:preview-ra4-gifs.weather.gov, DNS:preview-radar.weather.gov, DNS:preview-spot.weather.gov, DNS:preview.weather.gov, DNS:ra4-gifs-idp.weather.gov, DNS:ra4-gifs.weather.gov, DNS:radar.weather.gov, DNS:ripcurrents.noaa.gov, DNS:spot.weather.gov, DNS:tsunami.gov, DNS:water.weather.gov, DNS:wcatwc.arh.noaa.gov, DNS:weather.gov, DNS:www.nws.noaa.gov, DNS:www.ripcurrents.noaa.gov, DNS:www.tsunami.gov, DNS:www.weather.gov, DNS:www.wrh.noaa.gov, DNS:wwwx.wrh.noaa.gov
//              X509v3 Certificate Policies:
//                  Policy: 2.23.140.1.2.1
//              X509v3 CRL Distribution Points:
//                  Full Name:
//                    URI:http://r11.c.lencr.org/62.crl
//              CT Precertificate SCTs:
//                  Signed Certificate Timestamp:
//                      Version   : v1 (0x0)
//                      Log ID    : CC:FB:0F:6A:85:71:09:65:FE:95:9B:53:CE:E9:B2:7C:
//                                  22:E9:85:5C:0D:97:8D:B6:A9:7E:54:C0:FE:4C:0D:B0
//                      Timestamp : Apr 16 20:17:47.475 2025 GMT
//                      Extensions: none
//                      Signature : ecdsa-with-SHA256
//                                  30:45:02:21:00:87:C7:82:D2:71:13:9F:A6:84:4A:55:
//                                  D2:B0:0E:B0:3A:0E:AD:3A:CD:A9:51:1F:0A:92:58:A4:
//                                  9E:AE:D1:7A:33:02:20:66:F3:5E:63:A7:D6:A1:EB:84:
//                                  87:5C:86:A4:4F:D7:56:43:7A:AB:BF:25:4C:6A:E1:35:
//                                  BD:94:4D:92:8E:7B:DD
//                  Signed Certificate Timestamp:
//                      Version   : v1 (0x0)
//                      Log ID    : 12:F1:4E:34:BD:53:72:4C:84:06:19:C3:8F:3F:7A:13:
//                                  F8:E7:B5:62:87:88:9C:6D:30:05:84:EB:E5:86:26:3A
//                      Timestamp : Apr 16 20:17:51.447 2025 GMT
//                      Extensions: none
//                      Signature : ecdsa-with-SHA256
//                                  30:46:02:21:00:AE:2E:C7:96:9F:13:FA:4B:46:C9:B4:
//                                  63:73:A0:79:37:91:AE:1F:A8:5E:06:3A:90:5F:37:7A:
//                                  5F:A8:0B:6E:78:02:21:00:A2:A2:FB:D2:CE:D9:25:A2:
//                                  BE:FC:BF:D5:5D:D9:35:F7:6F:4E:37:E6:24:76:65:37:
//                                  89:6C:7C:96:8A:45:C8:C1
//      Signature Algorithm: sha256WithRSAEncryption
//      Signature Value:
//          77:45:35:90:d5:51:85:3e:fb:fc:dd:3b:59:d7:93:b3:65:d8:
//          c5:4e:f6:d0:bd:e8:44:17:3f:4a:de:db:62:48:74:3d:1c:3f:
//          0e:59:0d:34:14:0e:1c:bc:3d:b3:db:6f:2b:3f:54:2a:3b:0c:
//          c6:99:b6:c4:ef:4a:d7:fb:ee:00:cb:f2:1d:38:99:c1:9d:d1:
//          49:b5:b1:31:a9:fc:ca:c0:c4:e9:f4:9e:be:3a:ff:60:93:b9:
//          ea:3e:23:38:14:ea:a6:b3:62:04:3d:c8:ae:39:8b:be:1c:91:
//          57:f9:97:63:79:75:f9:15:58:3f:43:3c:39:03:99:ac:3b:88:
//          a4:e9:60:68:9e:1c:ff:41:f1:86:a3:ec:82:1e:8e:10:29:5d:
//          ed:2c:86:16:7f:3e:d7:3c:a5:e1:2f:6e:d0:30:0a:17:13:2e:
//          f7:58:84:34:ca:5f:ad:3e:ad:15:c5:6e:8a:db:84:26:64:af:
//          40:90:4b:86:ff:7f:c3:14:45:1f:e2:72:07:0d:20:3e:ba:9c:
//          94:3b:20:72:91:be:be:42:a3:b4:3b:be:14:02:69:ea:97:6e:
//          85:f4:7e:c6:f6:c4:aa:7a:23:47:9d:5b:75:ee:c9:b4:b5:04:
//          73:0b:86:6b:04:79:ea:af:f3:cc:dd:99:78:65:60:27:55:77:
//          c7:fe:73:57
//  -----BEGIN CERTIFICATE-----
//  MIIIxjCCB66gAwIBAgISBv1hLz7xQIvgmwPHG7AGcea9MA0GCSqGSIb3DQEBCwUA
//  MDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQwwCgYDVQQD
//  EwNSMTEwHhcNMjUwNDE2MTkxOTE3WhcNMjUwNzE1MTkxOTE2WjAWMRQwEgYDVQQD
//  Ewt3ZWF0aGVyLmdvdjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMNa
//  iBnRtP6+iqIV/be2gaQYhaezcHJEuCCb3IJW3onBYlk56TqDReKQ9iFif9FsaDq6
//  ursbPaq/3T0GqrQbSpltFwFUWMqwDGzXEiqEEGbOnG2nQsqlbb4PVgMiQ41PB4B4
//  YZgmHE1Np2lJAS44BUr8CoLYmView26X2ZkGICXdFj488K4yS/eSiip2aOjimTUb
//  IjPWsp/FKRlsB34ml5hz+SZr30EgSd4PO9Ohn4juyyxlef5UHWZTG6NEuf/swbPO
//  DDQNFqL/AP69ogJ60PE4xwERcjLqVus/voa3t6gfawifFBcoAlZrRXiM8FESQqVZ
//  uGBg/WUh6V+LBl+WwoMCAwEAAaOCBe8wggXrMA4GA1UdDwEB/wQEAwIFoDAdBgNV
//  HSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDAYDVR0TAQH/BAIwADAdBgNVHQ4E
//  FgQUBGD42OmyPi3kwnTVYTBRSLYgXp4wHwYDVR0jBBgwFoAUxc9GpOr0w8B6bJXE
//  LbBeki8m47kwVwYIKwYBBQUHAQEESzBJMCIGCCsGAQUFBzABhhZodHRwOi8vcjEx
//  Lm8ubGVuY3Iub3JnMCMGCCsGAQUFBzAChhdodHRwOi8vcjExLmkubGVuY3Iub3Jn
//  LzCCA8MGA1UdEQSCA7owggO2ghVhbGVydHMtdjIud2VhdGhlci5nb3aCEmFsZXJ0
//  cy53ZWF0aGVyLmdvdoITYW1kYXIubmNlcC5ub2FhLmdvdoIOYW1kYXIubm9hYS5n
//  b3aCFWFtZGFycWEubmNlcC5ub2FhLmdvdoIPYXBpLndlYXRoZXIuZ292ghRmb3Jl
//  Y2FzdC53ZWF0aGVyLmdvdoISaGFkcy5uY2VwLm5vYWEuZ292ghRoYWRzcWEubmNl
//  cC5ub2FhLmdvdoISaW53cy5uY2VwLm5vYWEuZ292ghRpbndzcWEubmNlcC5ub2Fh
//  LmdvdoISaXJpcy5uY2VwLm5vYWEuZ292ghRpcmlzcWEubmNlcC5ub2FhLmdvdoIT
//  bWFkaXMubmNlcC5ub2FhLmdvdoIObWFkaXMubm9hYS5nb3aCFW1hZGlzcWEubmNl
//  cC5ub2FhLmdvdoISbWFyaW5lLndlYXRoZXIuZ292ghJuYXR3Yy5hcmgubm9hYS5n
//  b3aCFW9wZW5nZW8ubmNlcC5ub2FhLmdvdoIacHJldmlldy1hbGVydHMud2VhdGhl
//  ci5nb3aCF3ByZXZpZXctYXBpLndlYXRoZXIuZ292ghxwcmV2aWV3LWZvcmVjYXN0
//  LndlYXRoZXIuZ292ghdwcmV2aWV3LWlkcC53ZWF0aGVyLmdvdoIacHJldmlldy1t
//  YXJpbmUud2VhdGhlci5nb3aCHXByZXZpZXctb3Blbmdlby5uY2VwLm5vYWEuZ292
//  ghxwcmV2aWV3LXJhNC1naWZzLndlYXRoZXIuZ292ghlwcmV2aWV3LXJhZGFyLndl
//  YXRoZXIuZ292ghhwcmV2aWV3LXNwb3Qud2VhdGhlci5nb3aCE3ByZXZpZXcud2Vh
//  dGhlci5nb3aCGHJhNC1naWZzLWlkcC53ZWF0aGVyLmdvdoIUcmE0LWdpZnMud2Vh
//  dGhlci5nb3aCEXJhZGFyLndlYXRoZXIuZ292ghRyaXBjdXJyZW50cy5ub2FhLmdv
//  doIQc3BvdC53ZWF0aGVyLmdvdoILdHN1bmFtaS5nb3aCEXdhdGVyLndlYXRoZXIu
//  Z292ghN3Y2F0d2MuYXJoLm5vYWEuZ292ggt3ZWF0aGVyLmdvdoIQd3d3Lm53cy5u
//  b2FhLmdvdoIYd3d3LnJpcGN1cnJlbnRzLm5vYWEuZ292gg93d3cudHN1bmFtaS5n
//  b3aCD3d3dy53ZWF0aGVyLmdvdoIQd3d3LndyaC5ub2FhLmdvdoIRd3d3eC53cmgu
//  bm9hYS5nb3YwEwYDVR0gBAwwCjAIBgZngQwBAgEwLgYDVR0fBCcwJTAjoCGgH4Yd
//  aHR0cDovL3IxMS5jLmxlbmNyLm9yZy82Mi5jcmwwggEFBgorBgEEAdZ5AgQCBIH2
//  BIHzAPEAdgDM+w9qhXEJZf6Vm1PO6bJ8IumFXA2XjbapflTA/kwNsAAAAZZAQCPT
//  AAAEAwBHMEUCIQCHx4LScROfpoRKVdKwDrA6Dq06zalRHwqSWKSertF6MwIgZvNe
//  Y6fWoeuEh1yGpE/XVkN6q78lTGrhNb2UTZKOe90AdwAS8U40vVNyTIQGGcOPP3oT
//  +Oe1YoeInG0wBYTr5YYmOgAAAZZAQDNXAAAEAwBIMEYCIQCuLseWnxP6S0bJtGNz
//  oHk3ka4fqF4GOpBfN3pfqAtueAIhAKKi+9LO2SWivvy/1V3ZNfdvTjfmJHZlN4ls
//  fJaKRcjBMA0GCSqGSIb3DQEBCwUAA4IBAQB3RTWQ1VGFPvv83TtZ15OzZdjFTvbQ
//  vehEFz9K3ttiSHQ9HD8OWQ00FA4cvD2z228rP1QqOwzGmbbE70rX++4Ay/IdOJnB
//  ndFJtbExqfzKwMTp9J6+Ov9gk7nqPiM4FOqms2IEPciuOYu+HJFX+ZdjeXX5FVg/
//  Qzw5A5msO4ik6WBonhz/QfGGo+yCHo4QKV3tLIYWfz7XPKXhL27QMAoXEy73WIQ0
//  yl+tPq0VxW6K24QmZK9AkEuG/3/DFEUf4nIHDSA+upyUOyBykb6+QqO0O74UAmnq
//  l26F9H7G9sSqeiNHnVt17sm0tQRzC4ZrBHnqr/PM3Zl4ZWAnVXfH/nNX
//  -----END CERTIFICATE-----
//
//  Certificate:
//      Data:
//          Version: 3 (0x2)
//          Serial Number:
//              8a:7d:3e:13:d6:2f:30:ef:23:86:bd:29:07:6b:34:f8
//          Signature Algorithm: sha256WithRSAEncryption
//          Issuer: C = US, O = Internet Security Research Group, CN = ISRG Root X1
//          Validity
//              Not Before: Mar 13 00:00:00 2024 GMT
//              Not After : Mar 12 23:59:59 2027 GMT
//          Subject: C = US, O = Let's Encrypt, CN = R11
//          Subject Public Key Info:
//              Public Key Algorithm: rsaEncryption
//                  Public-Key: (2048 bit)
//                  Modulus:
//                      00:ba:87:bc:5c:1b:00:39:cb:ca:0a:cd:d4:67:10:
//                      f9:01:3c:a5:4e:a5:61:cb:26:ca:52:fb:15:01:b7:
//                      b9:28:f5:28:1e:ed:27:b3:24:18:39:67:09:0c:08:
//                      ec:e0:3a:b0:3b:77:0e:bd:f3:e5:39:54:41:0c:4e:
//                      ae:41:d6:99:74:de:51:db:ef:7b:ff:58:bd:a8:b7:
//                      13:f6:de:31:d5:f2:72:c9:72:6a:0b:83:74:95:9c:
//                      46:00:64:14:99:f3:b1:d9:22:d9:cd:a8:92:aa:1c:
//                      26:7a:3f:fe:ef:58:05:7b:08:95:81:db:71:0f:8e:
//                      fb:e3:31:09:bb:09:be:50:4d:5f:8f:91:76:3d:5a:
//                      9d:9e:83:f2:e9:c4:66:b3:e1:06:66:43:48:18:80:
//                      65:a0:37:18:9a:9b:84:32:97:b1:b2:bd:c4:f8:15:
//                      00:9d:27:88:fb:e2:63:17:96:6c:9b:27:67:4b:c4:
//                      db:28:5e:69:c2:79:f0:49:5c:e0:24:50:e1:c4:bc:
//                      a1:05:ac:7b:40:6d:00:b4:c2:41:3f:a7:58:b8:2f:
//                      c5:5c:9b:a5:bb:09:9e:f1:fe:eb:b0:85:39:fd:a8:
//                      0a:ef:45:c4:78:eb:65:2a:c2:cf:5f:3c:de:e3:5c:
//                      4d:1b:f7:0b:27:2b:aa:0b:42:77:53:4f:79:6a:1d:
//                      87:d9
//                  Exponent: 65537 (0x10001)
//          X509v3 extensions:
//              X509v3 Key Usage: critical
//                  Digital Signature, Certificate Sign, CRL Sign
//              X509v3 Extended Key Usage:
//                  TLS Web Client Authentication, TLS Web Server Authentication
//              X509v3 Basic Constraints: critical
//                  CA:TRUE, pathlen:0
//              X509v3 Subject Key Identifier:
//                  C5:CF:46:A4:EA:F4:C3:C0:7A:6C:95:C4:2D:B0:5E:92:2F:26:E3:B9
//              X509v3 Authority Key Identifier:
//                  79:B4:59:E6:7B:B6:E5:E4:01:73:80:08:88:C8:1A:58:F6:E9:9B:6E
//              Authority Information Access:
//                  CA Issuers - URI:http://x1.i.lencr.org/
//              X509v3 Certificate Policies:
//                  Policy: 2.23.140.1.2.1
//              X509v3 CRL Distribution Points:
//                  Full Name:
//                    URI:http://x1.c.lencr.org/
//      Signature Algorithm: sha256WithRSAEncryption
//      Signature Value:
//          4e:e2:89:5d:0a:03:1c:90:38:d0:f5:1f:f9:71:5c:f8:c3:8f:
//          b2:37:88:7a:6f:b0:25:1f:ed:be:b7:d8:86:06:8e:e9:09:84:
//          cd:72:bf:81:f3:fc:ca:cf:53:48:ed:bd:f6:69:42:d4:a5:11:
//          3e:35:c8:13:b2:92:1d:05:5f:ea:2e:d4:d8:f8:49:c3:ad:f5:
//          99:96:9c:ef:26:d8:e1:b4:24:0b:48:20:4d:fc:d3:54:b4:a9:
//          c6:21:c8:e1:36:1b:ff:77:64:29:17:b9:f0:4b:ef:5d:ea:cd:
//          79:d0:bf:90:bf:be:23:b2:90:da:4a:a9:48:31:74:a9:44:0b:
//          e1:e2:f6:2d:83:71:a4:75:7b:d2:94:c1:05:19:46:1c:b9:8f:
//          f3:c4:74:48:25:2a:0d:e5:f5:db:43:e2:db:93:9b:b9:19:b4:
//          1f:2f:df:6a:0e:8f:31:d3:63:0f:bb:29:dc:dd:66:2c:3f:b0:
//          1b:67:51:f8:41:3c:e4:4d:b9:ac:b8:a4:9c:66:63:f5:ab:85:
//          23:1d:cc:53:b6:ab:71:ae:dc:c5:01:71:da:36:ee:0a:18:2a:
//          32:fd:09:31:7c:8f:f6:73:e7:9c:9c:b5:4a:15:6a:77:82:5a:
//          cf:da:8d:45:fe:1f:2a:64:05:30:3e:73:c2:c6:0c:b9:d6:3b:
//          63:4a:ab:46:03:fe:99:c0:46:40:27:60:63:df:50:3a:07:47:
//          d8:15:4a:9f:ea:47:1f:99:5a:08:62:0c:b6:6c:33:08:4d:d7:
//          38:ed:48:2d:2e:05:68:ae:80:5d:ef:4c:dc:d8:20:41:5f:68:
//          f1:bb:5a:cd:e3:0e:b0:0c:31:87:9b:43:de:49:43:e1:c8:04:
//          3f:d1:3c:1b:87:45:30:69:a8:a9:72:0e:79:12:1c:31:d8:3e:
//          23:57:dd:a7:4f:a0:f0:1c:81:d1:77:1f:6f:d6:d2:b9:a8:b3:
//          03:16:81:39:4b:9f:55:ae:d2:6a:e4:b3:bf:ea:a5:d5:9f:4b:
//          a3:c9:d6:3b:72:f3:4a:f6:54:ab:0c:fc:38:f7:60:80:df:6e:
//          35:ca:75:a1:54:e4:2f:bc:6e:17:c9:1a:a5:37:b5:a2:9a:ba:
//          ec:f4:c0:75:46:4f:77:a8:e8:59:56:91:66:2d:6e:de:29:81:
//          d6:a6:97:05:5e:64:45:be:2c:ce:ea:64:42:44:b0:c3:4f:ad:
//          f0:b4:dc:03:ca:99:9b:09:82:95:82:0d:63:8a:66:f9:19:72:
//          f8:d5:b9:89:10:e2:89:98:09:35:f9:a2:1c:be:92:73:23:74:
//          e9:9d:1f:d7:3b:4a:9a:84:58:10:c2:f3:a7:e2:35:ec:7e:3b:
//          45:ce:30:46:52:6b:c0:c0
//  -----BEGIN CERTIFICATE-----
//  MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAw
//  TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
//  cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw
//  WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg
//  RW5jcnlwdDEMMAoGA1UEAxMDUjExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB
//  CgKCAQEAuoe8XBsAOcvKCs3UZxD5ATylTqVhyybKUvsVAbe5KPUoHu0nsyQYOWcJ
//  DAjs4DqwO3cOvfPlOVRBDE6uQdaZdN5R2+97/1i9qLcT9t4x1fJyyXJqC4N0lZxG
//  AGQUmfOx2SLZzaiSqhwmej/+71gFewiVgdtxD4774zEJuwm+UE1fj5F2PVqdnoPy
//  6cRms+EGZkNIGIBloDcYmpuEMpexsr3E+BUAnSeI++JjF5ZsmydnS8TbKF5pwnnw
//  SVzgJFDhxLyhBax7QG0AtMJBP6dYuC/FXJuluwme8f7rsIU5/agK70XEeOtlKsLP
//  Xzze41xNG/cLJyuqC0J3U095ah2H2QIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB
//  hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB
//  /wIBADAdBgNVHQ4EFgQUxc9GpOr0w8B6bJXELbBeki8m47kwHwYDVR0jBBgwFoAU
//  ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC
//  hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG
//  A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN
//  AQELBQADggIBAE7iiV0KAxyQOND1H/lxXPjDj7I3iHpvsCUf7b632IYGjukJhM1y
//  v4Hz/MrPU0jtvfZpQtSlET41yBOykh0FX+ou1Nj4ScOt9ZmWnO8m2OG0JAtIIE38
//  01S0qcYhyOE2G/93ZCkXufBL713qzXnQv5C/viOykNpKqUgxdKlEC+Hi9i2DcaR1
//  e9KUwQUZRhy5j/PEdEglKg3l9dtD4tuTm7kZtB8v32oOjzHTYw+7KdzdZiw/sBtn
//  UfhBPORNuay4pJxmY/WrhSMdzFO2q3Gu3MUBcdo27goYKjL9CTF8j/Zz55yctUoV
//  aneCWs/ajUX+HypkBTA+c8LGDLnWO2NKq0YD/pnARkAnYGPfUDoHR9gVSp/qRx+Z
//  WghiDLZsMwhN1zjtSC0uBWiugF3vTNzYIEFfaPG7Ws3jDrAMMYebQ95JQ+HIBD/R
//  PBuHRTBpqKlyDnkSHDHYPiNX3adPoPAcgdF3H2/W0rmoswMWgTlLn1Wu0mrks7/q
//  pdWfS6PJ1jty80r2VKsM/Dj3YIDfbjXKdaFU5C+8bhfJGqU3taKauuz0wHVGT3eo
//  6FlWkWYtbt4pgdamlwVeZEW+LM7qZEJEsMNPrfC03APKmZsJgpWCDWOKZvkZcvjV
//  uYkQ4omYCTX5ohy+knMjdOmdH9c7SpqEWBDC86fiNex+O0XOMEZSa8DA
//  -----END CERTIFICATE-----
//
// clang-format on

namespace {
const char PROGMEM r10_[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFBTCCAu2gAwIBAgIQS6hSk/eaL6JzBkuoBI110DANBgkqhkiG9w0BAQsFADBP\n"
    "MQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFy\n"
    "Y2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMTAeFw0yNDAzMTMwMDAwMDBa\n"
    "Fw0yNzAzMTIyMzU5NTlaMDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBF\n"
    "bmNyeXB0MQwwCgYDVQQDEwNSMTAwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n"
    "AoIBAQDPV+XmxFQS7bRH/sknWHZGUCiMHT6I3wWd1bUYKb3dtVq/+vbOo76vACFL\n"
    "YlpaPAEvxVgD9on/jhFD68G14BQHlo9vH9fnuoE5CXVlt8KvGFs3Jijno/QHK20a\n"
    "/6tYvJWuQP/py1fEtVt/eA0YYbwX51TGu0mRzW4Y0YCF7qZlNrx06rxQTOr8IfM4\n"
    "FpOUurDTazgGzRYSespSdcitdrLCnF2YRVxvYXvGLe48E1KGAdlX5jgc3421H5KR\n"
    "mudKHMxFqHJV8LDmowfs/acbZp4/SItxhHFYyTr6717yW0QrPHTnj7JHwQdqzZq3\n"
    "DZb3EoEmUVQK7GH29/Xi8orIlQ2NAgMBAAGjgfgwgfUwDgYDVR0PAQH/BAQDAgGG\n"
    "MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDATASBgNVHRMBAf8ECDAGAQH/\n"
    "AgEAMB0GA1UdDgQWBBS7vMNHpeS8qcbDpHIMEI2iNeHI6DAfBgNVHSMEGDAWgBR5\n"
    "tFnme7bl5AFzgAiIyBpY9umbbjAyBggrBgEFBQcBAQQmMCQwIgYIKwYBBQUHMAKG\n"
    "Fmh0dHA6Ly94MS5pLmxlbmNyLm9yZy8wEwYDVR0gBAwwCjAIBgZngQwBAgEwJwYD\n"
    "VR0fBCAwHjAcoBqgGIYWaHR0cDovL3gxLmMubGVuY3Iub3JnLzANBgkqhkiG9w0B\n"
    "AQsFAAOCAgEAkrHnQTfreZ2B5s3iJeE6IOmQRJWjgVzPw139vaBw1bGWKCIL0vIo\n"
    "zwzn1OZDjCQiHcFCktEJr59L9MhwTyAWsVrdAfYf+B9haxQnsHKNY67u4s5Lzzfd\n"
    "u6PUzeetUK29v+PsPmI2cJkxp+iN3epi4hKu9ZzUPSwMqtCceb7qPVxEbpYxY1p9\n"
    "1n5PJKBLBX9eb9LU6l8zSxPWV7bK3lG4XaMJgnT9x3ies7msFtpKK5bDtotij/l0\n"
    "GaKeA97pb5uwD9KgWvaFXMIEt8jVTjLEvwRdvCn294GPDF08U8lAkIv7tghluaQh\n"
    "1QnlE4SEN4LOECj8dsIGJXpGUk3aU3KkJz9icKy+aUgA+2cP21uh6NcDIS3XyfaZ\n"
    "QjmDQ993ChII8SXWupQZVBiIpcWO4RqZk3lr7Bz5MUCwzDIA359e57SSq5CCkY0N\n"
    "4B6Vulk7LktfwrdGNVI5BsC9qqxSwSKgRJeZ9wygIaehbHFHFhcBaMDKpiZlBHyz\n"
    "rsnnlFXCb5s8HKn5LsUgGvB24L7sGNZP2CX7dhHov+YhD+jozLW2p9W4959Bz2Ei\n"
    "RmqDtmiXLnzqTpXbI+suyCsohKRg6Un0RC47+cpiVwHiXZAW+cn8eiNIjqbVgXLx\n"
    "KPpdzvvtTnOPlC7SQZSYmdunr3Bf9b77AiC/ZidstK36dRILKz7OA54=\n"
    "-----END CERTIFICATE-----\n";

const char PROGMEM r11_[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFBjCCAu6gAwIBAgIRAIp9PhPWLzDvI4a9KQdrNPgwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n"
    "WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n"
    "RW5jcnlwdDEMMAoGA1UEAxMDUjExMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n"
    "CgKCAQEAuoe8XBsAOcvKCs3UZxD5ATylTqVhyybKUvsVAbe5KPUoHu0nsyQYOWcJ\n"
    "DAjs4DqwO3cOvfPlOVRBDE6uQdaZdN5R2+97/1i9qLcT9t4x1fJyyXJqC4N0lZxG\n"
    "AGQUmfOx2SLZzaiSqhwmej/+71gFewiVgdtxD4774zEJuwm+UE1fj5F2PVqdnoPy\n"
    "6cRms+EGZkNIGIBloDcYmpuEMpexsr3E+BUAnSeI++JjF5ZsmydnS8TbKF5pwnnw\n"
    "SVzgJFDhxLyhBax7QG0AtMJBP6dYuC/FXJuluwme8f7rsIU5/agK70XEeOtlKsLP\n"
    "Xzze41xNG/cLJyuqC0J3U095ah2H2QIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB\n"
    "hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB\n"
    "/wIBADAdBgNVHQ4EFgQUxc9GpOr0w8B6bJXELbBeki8m47kwHwYDVR0jBBgwFoAU\n"
    "ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC\n"
    "hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG\n"
    "A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN\n"
    "AQELBQADggIBAE7iiV0KAxyQOND1H/lxXPjDj7I3iHpvsCUf7b632IYGjukJhM1y\n"
    "v4Hz/MrPU0jtvfZpQtSlET41yBOykh0FX+ou1Nj4ScOt9ZmWnO8m2OG0JAtIIE38\n"
    "01S0qcYhyOE2G/93ZCkXufBL713qzXnQv5C/viOykNpKqUgxdKlEC+Hi9i2DcaR1\n"
    "e9KUwQUZRhy5j/PEdEglKg3l9dtD4tuTm7kZtB8v32oOjzHTYw+7KdzdZiw/sBtn\n"
    "UfhBPORNuay4pJxmY/WrhSMdzFO2q3Gu3MUBcdo27goYKjL9CTF8j/Zz55yctUoV\n"
    "aneCWs/ajUX+HypkBTA+c8LGDLnWO2NKq0YD/pnARkAnYGPfUDoHR9gVSp/qRx+Z\n"
    "WghiDLZsMwhN1zjtSC0uBWiugF3vTNzYIEFfaPG7Ws3jDrAMMYebQ95JQ+HIBD/R\n"
    "PBuHRTBpqKlyDnkSHDHYPiNX3adPoPAcgdF3H2/W0rmoswMWgTlLn1Wu0mrks7/q\n"
    "pdWfS6PJ1jty80r2VKsM/Dj3YIDfbjXKdaFU5C+8bhfJGqU3taKauuz0wHVGT3eo\n"
    "6FlWkWYtbt4pgdamlwVeZEW+LM7qZEJEsMNPrfC03APKmZsJgpWCDWOKZvkZcvjV\n"
    "uYkQ4omYCTX5ohy+knMjdOmdH9c7SpqEWBDC86fiNex+O0XOMEZSa8DA\n"
    "-----END CERTIFICATE-----\n";

const char PROGMEM digicert_global_g3_tls_ecc_sha384_2020_ca1_[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDeTCCAv+gAwIBAgIQCwDpLU1tcx/KMFnHyx4YhjAKBggqhkjOPQQDAzBhMQsw\n"
    "CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\n"
    "ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\n"
    "Fw0yMTA0MTQwMDAwMDBaFw0zMTA0MTMyMzU5NTlaMFkxCzAJBgNVBAYTAlVTMRUw\n"
    "EwYDVQQKEwxEaWdpQ2VydCBJbmMxMzAxBgNVBAMTKkRpZ2lDZXJ0IEdsb2JhbCBH\n"
    "MyBUTFMgRUNDIFNIQTM4NCAyMDIwIENBMTB2MBAGByqGSM49AgEGBSuBBAAiA2IA\n"
    "BHipnHWuiF1jpK1dhtgQSdavklljQyOF9EhlMM1KNJWmDj7ZfAjXVwUoSJ4Lq+vC\n"
    "05ae7UXSi4rOAUsXQ+Fzz21zSDTcAEYJtVZUyV96xxMH0GwYF2zK28cLJlYujQf1\n"
    "Z6OCAYIwggF+MBIGA1UdEwEB/wQIMAYBAf8CAQAwHQYDVR0OBBYEFIoj655r1/k3\n"
    "XfltITl2mqFn3hCoMB8GA1UdIwQYMBaAFLPbSKT5ocXYrjZBzBFjaWIpvEvGMA4G\n"
    "A1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdgYI\n"
    "KwYBBQUHAQEEajBoMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5j\n"
    "b20wQAYIKwYBBQUHMAKGNGh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdp\n"
    "Q2VydEdsb2JhbFJvb3RHMy5jcnQwQgYDVR0fBDswOTA3oDWgM4YxaHR0cDovL2Ny\n"
    "bDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9vdEczLmNybDA9BgNVHSAE\n"
    "NjA0MAsGCWCGSAGG/WwCATAHBgVngQwBATAIBgZngQwBAgEwCAYGZ4EMAQICMAgG\n"
    "BmeBDAECAzAKBggqhkjOPQQDAwNoADBlAjB+Jlhu7ojsDN0VQe56uJmZcNFiZU+g\n"
    "IJ5HsVvBsmcxHcxyeq8ickBCbmWE/odLDxkCMQDmv9auNIdbP2fHHahv1RJ4teaH\n"
    "MUSpXca4eMzP79QyWBH/OoUGPB2Eb9P1+dozHKQ=\n"
    "-----END CERTIFICATE-----\n";

}  // namespace

const std::map<String, const char *> CACerts::ca_certs{
    {"api.weather.gov", r10_},
    {"example.com", digicert_global_g3_tls_ecc_sha384_2020_ca1_},
};
