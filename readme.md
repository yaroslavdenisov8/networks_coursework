# Курсовая работа по дисциплине «Компьютерные сети», вариант 2

## Сборка

```sh
make
```

## Запуск

```sh
./server <directory> [port] [workers]
```

Например:

```sh
./server ./www 8080 4
```

После запуска сервер доступен по адресу <http://127.0.0.1:8080/>. Журнал событий записывается в `server.log`. Остановка выполняется через `Ctrl+C`.

## Проверка HTTP-статусов

```sh
# 200: GET существующего файла
curl -sS -o /dev/null -w 'GET  /test.txt       -> %{http_code}\n' http://127.0.0.1:8080/test.txt

# 200: HEAD существующего файла; тело ответа не передаётся
curl -sS -o /dev/null -w 'HEAD /test.txt       -> %{http_code}\n' -I http://127.0.0.1:8080/test.txt

# 404: отсутствующий файл
curl -sS -o /dev/null -w 'GET  /missing.txt    -> %{http_code}\n' http://127.0.0.1:8080/missing.txt

# 403: попытка выйти за пределы корневой директории
curl --path-as-is -sS -o /dev/null -w 'GET  /../../etc/passwd -> %{http_code}\n' http://127.0.0.1:8080/../../etc/passwd

# 405: неподдерживаемый метод POST; сервер разрешает только GET и HEAD
curl -sS -o /dev/null -w 'POST /test.txt       -> %{http_code}\n' -X POST http://127.0.0.1:8080/test.txt
```
