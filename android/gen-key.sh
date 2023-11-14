#!/usr/bin/sh

#using method from https://source.android.com/docs/core/ota/sign_builds
# ... which is actually for OS images but appears to be correct for the apk keys as well

#keypair:
openssl genrsa -3 -out temp.pem 2048

#certificate in x509 format:
openssl req -new -x509 -key temp.pem -out testkey.x509.pem -days 10000 -subj '/C=US/ST=Pennsylvania/L=Pittsburgh/O=15-466/OU=15-466-f23/CN=gp23/emailAddress=none@example.com'

#private key in PKCS#8 format:
openssl pkcs8 -in temp.pem -topk8 -outform DER -out testkey.pk8 -nocrypt

#temp.pem no longer needed:
rm temp.pem
