# USI TWI Clock Stretching — журнал попыток

## Контекст

ATtiny85 USI TWI slave на одной I2C шине с Mega master и SSD1306 OLED.  
Нужно: слейвы передают данные мастеру, OLED отображает нормально.

**Текущее рабочее состояние** (коммит `baaea5f`/`64b656f`):  
- I2C между Mega и ATtiny85 работает ✅  
- OLED ломается: сдвиг изображения + `OledController::readByte Control Byte Error` + `I2C bus recovery` ❌

---

## Попытка 1 — Soft clock stretch (флаг, без физического пина)

**Что сделано:**  
- `stepCounter()`: при overflow просто `m_sclHold = true`, пин не трогается  
- `voltChanged()`: добавлен `if( m_sclHold ) return;` после `if( m_clkState == clkState ) return;`  
- `configureB()`: `m_sclHold = false` + ресинк `m_clkState = m_CKpin->getInpState()`

**Результат:**  
- OLED работает нормально ✅  
- I2C между Mega и слейвами не работает — мастер читает `0xFF` (или `0x7FFF`/`0x00FF`) ❌

**Почему не сработало:**  
Неизвестно. Несмотря на то что флаг корректно блокирует обработку SCL фронтов, слейв отдаёт неверные данные. Возможная причина: мастер не ждёт пока слейв освободит шину (SCL физически не удерживается), поэтому читает следующий байт раньше чем слейв его выставил. Без физического удержания SCL мастер не "знает" что нужно ждать.

---

## Попытка 2 — setOutState(true) перед controlPin(false, false)

**Что сделано:**  
В `configureB()` перед `controlPin(false, false)` добавлен `m_CKpin->setOutState(true)`.

**Результат:**  
- Без изменений. OLED по-прежнему ломается ❌

**Почему не сработало:**  
`controlPin(false, false)` на input-mode пине (TWI open-collector) в ветке `else` делает `m_outState = m_portState`. `m_portState` у ATtiny85 в TWI режиме = 0 (open-collector PORT=0). Поэтому после release пин снова получает LOW, затирая наш `setOutState(true)`.

---

## Попытка 3 — scheduleState(true, 0) после controlPin(false, false)

**Что сделано:**  
В `configureB()` после `controlPin(false, false)` добавлен `m_CKpin->scheduleState(true, 0)`.

**Результат:**  
- Без изменений. OLED по-прежнему ломается ❌

**Почему не сработало:**  
`McuPin::scheduleState` содержит guard: `if( m_outCtrl ) IoPin::scheduleState(...)`. После `controlPin(false, false)` `m_outCtrl` уже `false` — вызов игнорируется.

---

## Попытка 4 — scheduleState(true, 0) ДО controlPin(false, false)

**Что сделано:**  
В `configureB()`: `setOutState(true)` → `scheduleState(true, 0)` → `controlPin(false, false)`.

**Результат:**  
- Без изменений. OLED по-прежнему ломается ❌

**Почему не сработало:**  
`scheduleState` вызывается пока `m_outCtrl=true`, но после `controlPin(false, false)` ветка `else` (input-mode) всё равно делает `m_outState = m_portState = 0`, перезатирая результат.

---

## Текущее состояние кода

Физический clock stretch (попытка 1 в обратную сторону):  
- `stepCounter()`: `controlPin(true, false)` + `setOutState(false)` — физически тянет SCL LOW  
- `configureB()`: `controlPin(false, false)` — отпускает (но SCL остаётся LOW из-за input-mode bug)

I2C слейвы работают ✅, OLED ломается ❌.

---

## Корень проблемы

`McuPin::controlPin(false, false)` при `m_pinMode == input`:
```cpp
if( m_pinMode > input ) scheduleState( m_portState, 0 ); // НЕ выполняется
else                    m_outState = m_portState;         // m_portState=0 → SCL остаётся LOW
```
SCL физически не поднимается после release. OLED видит постоянный LOW на SCL во время своих транзакций.

---

## Попытка 5 — setExtraSource (admittance pull-to-GND)

**Что сделано:**  
- `stepCounter()`: `m_CKpin->setExtraSource( 0, 1/1e-9 )` — сильный pull-down через admittance, без захвата пина
- `configureB()`: `m_CKpin->setExtraSource( 0, 0 )` — убирает pull-down (gndAdmit=0)

**Почему должно работать:**  
Admittance pull не захватывает пин (`m_outCtrl` остаётся `false`), поэтому другие девайсы на шине (OLED) могут поднимать SCL. Это настоящий open-drain на уровне симулятора.

**Результат:**  
- OLED работает нормально ✅
- I2C слейвы отдают данные ✅
- 0 I2C bus recovery ✅

**Почему сработало:**  
Два отдельных fix-а вместе:
1. `setExtraSource(0, 1e9)` правильно тянет SCL к GND без захвата пина — OLED может поднять SCL самостоятельно
2. Stretch делается только при `m_interrupt->enabled()` — иначе ISR не напишет USISR и SCL останется LOW навсегда
3. Добавлен `m_waitSCL` в `TwiModule` — master ждёт когда slave отпустит SCL вместо того чтобы игнорировать stretch

---

## Идеи не опробованные

- [ ] Патч в `McuPin::controlPin`: для input-mode тоже вызывать `scheduleState` вместо прямого присвоения
- [ ] Разобраться почему в soft-stretch слейвы отдают 0xFF — решить эту проблему и вернуться к soft-stretch
