Здесь хранится ваш личный ключ подписи `.sisx`. Его **создаёт сам `build.py`** при первой сборке (на чистом Python, openssl не нужен). В репозиторий он не попадает — никому его не давайте.

Если хотите сделать ключ вручную:

```
openssl req -x509 -newkey rsa:1024 -sha1 -nodes -days 14600 \
  -keyout cs16e52_key.pem -out cs16e52_cert.pem -subj "/CN=CS16 E52"
```
