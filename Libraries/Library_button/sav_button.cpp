#include "sav_button.h"

/**
 * Конструктор класса кнопки
 * Кнопка это цифровой пин подтянутый к питанию и замыкаемый на землю
 * Событие срабатывания происходит по нажатию кнопки (возвращается 1)
 * и отпусканию кнопки (возвращается время нажатия кнопки, мсек)
 * tm1 - таймаут дребезга контактов. По умолчанию 50мс
 * tm2 - время длинного нажатия клавиши. По умолчанию 2000мс
 * tm3 - врямы перевода кнопки в генерацию серии нажатий. По умолсанию отключено
 * tm4 - время между кликами в серии. По умолчанию 500 мс. Если tm3 = 0 то не работает
 */
SButton::SButton(uint8_t pin,uint16_t val,uint16_t tm1, uint16_t tm2,uint16_t tm3, uint16_t tm4){
   Pin               = pin;
   State             = false;
   Long_press_state  = false;
   Auto_press_state  = false;
   Ms1               = 0;
   Ms2               = 0;
   Ms_auto_click     = 0;
   TM_bounce         = tm1;
   TM_long_click     = tm2;
   TM_auto_click     = tm3;
   Period_auto_click = tm4;
   value             = val;
}
/**
 * Инициализация кнопки
 */
void SButton::begin() {
   pinMode(Pin, INPUT_PULLUP);  
}

/**
 * Действие производимое в цикле или по таймеру
 * возвращает SB_NONE если кнопка не нажата и событие нажатие или длинного нажатия кнопки
*/
SBUTTON_CLICK SButton::Loop() {
   uint32_t ms = millis();
   uint16_t pin_value = analogRead(Pin); //читаем сигнал с пина где висит кнопка
   bool pin_state = false;
   if((pin_value >= value-error) && (pin_value <= value + error)){
      pin_state = HIGH; //если напряжение с кнопки лежит в наших ожидаемых пределах то ставим pin_state в HIGH
   }
   else pin_state = LOW; //если нет,может быть нажаты другие кнопки или не нажаты вовсе это LOW для нашей конкретной кнопки
// Фиксируем нажатие кнопки 
   if( pin_state == HIGH && State == false && (ms-Ms1)>TM_bounce ){
       Long_press_state = false;
       Auto_press_state = false;
       State = true;
       Ms2    = ms;
       if( TM_long_click == 0 && TM_auto_click == 0 ) return SB_CLICK;
   }

// Фиксируем длинное нажатие кнопки   
   if( pin_state == HIGH && !Long_press_state && TM_long_click>0 && ( ms - Ms2 )>TM_long_click ){
      Long_press_state = true;
      return SB_LONG_CLICK;
   }

// Фиксируем авто нажатие кнопки   
   if( pin_state == HIGH && TM_auto_click > 0 && ( ms - Ms2 ) > TM_auto_click && ( ms - Ms_auto_click ) > Period_auto_click ){
      Auto_press_state = true;
      Ms_auto_click    = ms;   
      return SB_AUTO_CLICK;
   }

   
// Фиксируем отпускание кнопки 
   if( pin_state == LOW && State == true  && (ms-Ms2)>TM_bounce ){
// Сбрасываем все флаги       
       State            = false;
      Ms1    = ms;
// Возвращаем короткий клик      
      if( (TM_long_click != 0 || TM_auto_click != 0) && !Long_press_state && !Auto_press_state ) return SB_CLICK; 
   }
   return SB_NONE;
}     

