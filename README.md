# Застосунок керування для інформаційного табло для ПК
## Побудова
Аби побудувати застосунок потрібні компілятор gcc та ОС Linux
Використовується наступна команда:
```
gcc -Wall main.c info_sources.c -o tabloctl
```
# Використання:
Запустити застосунок:
```
tabloctl
```
Запустити в фоновому режимі
```
tabloctl -d
```
Аби перезавантажити конфігурацію, можна надіслати застосунку сигнал `SIGUSR1`:
```
pkill -SIGUSR1 tabloctl
```