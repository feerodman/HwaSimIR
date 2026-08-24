mkdir ca && mkdir user && mkdir newcerts && touch index.txt && touch index.txt.attr && touch serial && echo 01 >> serial
openssl genrsa -out ca/democa.key 2048
openssl req -new -key ca/democa.key -subj "/C=CN/ST=JS/L=NJ/O=ZR/CN=demoCA" -out ca/democa.csr
openssl x509 -req -sha1 -in ca/democa.csr -signkey ca/democa.key -days 3650 -out ca/democa.pem

openssl genrsa -out user/app1.key 2048
openssl req -new -key user/app1.key -subj "/C=CN/ST=JS/L=NJ/O=ZR/CN=app1/emailAddress=app1.gmail.com" -out user/app1.csr
openssl ca -cert ./ca/democa.pem -keyfile ./ca/democa.key -config ./openssl.cnf -in user/app1.csr -out user/app1Cert.pem

openssl genrsa -out user/app2.key 2048
openssl req -new -key user/app2.key -subj "/C=CN/ST=JS/L=NJ/O=ZR/CN=app2/emailAddress=app2.gmail.com" -out user/app2.csr
openssl ca -cert ./ca/democa.pem -keyfile ./ca/democa.key -config ./openssl.cnf -in user/app2.csr -out user/app2Cert.pem

openssl smime -sign -signer ca/democa.pem -inkey ca/democa.key -in auth_permission.xml -out auth_permission.smime
openssl smime -sign -signer ca/democa.pem -inkey ca/democa.key -in auth_governance.xml -out auth_governance.smime