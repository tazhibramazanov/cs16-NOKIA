Сюда кладётся свой самоподписанный сертификат для `.sisx` (в репозиторий не входит):

```
openssl req -x509 -newkey rsa:1024 -sha1 -nodes -days 14600 \
  -keyout cs16e52_key.pem -out cs16e52_cert.pem -subj "/CN=CS16 E52"
```
