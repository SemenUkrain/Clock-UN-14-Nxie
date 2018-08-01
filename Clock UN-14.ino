#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <sav_button.h>
//Выводы дешифратора К155ИД1
#define A 8
#define B 9
#define C 10
#define D 11
//Выводы управление транзисторами на каждую цифру
#define hour1_pin 2
#define hour2_pin 3
#define minut1_pin 4
#define minut2_pin 5
#define second1_pin 6
#define second2_pin 7
//Для разделительной точки
#define dot_clock A7
//Для RGB светодиодов
#define red    A0
#define green  A1
#define blue    A2
//Кнопки 3шт через налоговый пин
#define buttons A4

#define settingsTime 5000 //время ожидания в режиме настройек 
#define changeTime 45000 //время показа часов 
#define changeData 8000  //время показа даты 
#define changeTemp 7000  //время показа температуры

unsigned long currentTime = 0; //текущие время для использование в функции динамической индикации
unsigned long nextflick = 0 ; // следующие время обновления изображения на лампмах
const  byte toflick = 3; // время горения одной лампы в (мс)
byte digit = 0 ;// переменная хранения разряда 0-десятки часов , 1-еденицы часов .... 5-еденицы секунд
byte NumNixie [6] = {0,0,0,0,0,0}; //Массив храения цифр текущего времени (десятки часов,еденицы часов,...,еденицы секунд)
byte Data [6] = {0,0,0,0,0,0}; //ассив для хранения даты
byte NumTransistors [6] = {hour1_pin,hour2_pin,minut1_pin,minut2_pin,second1_pin,second2_pin}; //массив хранения номеров пинов управляющих тразисторами анодами ламп

tmElements_t tm; //объект для работы с библиотекой времени микросхемы DS1307

SButton button_menu (buttons,527,50,2500,0,0); //кнопка меню
SButton button_up   (buttons,695,50,1500,2000,300); //кнопка вверх 
SButton button_down (buttons,1023,50,1500,2000,300); //кнопка вниз 1023-ожидаемое аналоговое значение 50мс-защита от дребезга 1500мс-длинное нажатие 2000-мс-переход в режим автонажатий

byte MachineState = 0; //Переменная состояния часов\ 0-отображение времени \1-отображение даты \2-отображение температуры \3-режим настройки времени\4-настр даты
byte SetDigit = 0;//переменная для хранения изменяемого разряда при настройке часов 
unsigned long LocatingMenu = 0;//Время входа в режим настройки 

unsigned long interval = 0;// переменна для хранения промежутка времени для циклического переключения отображения 

int SettingsNumber [6] = {0,0,0,0,0,0}; //ранение настроееного времени 
int SettingsData [6] = {0,0,0,0,0,0}; //хранение настроенной даты 
byte CountON = 0;//переменная счётчик для осуществления мигания выбранного разряда в режиме настройки 
bool CountOFF = false; // переменная флаг для осуществления мигания выбранного разряда в режиме настройки 

byte previosMinuts = 0;//Переменная для переключения анимации антиотравления каждую минуту


void NixieShow (byte a,byte b,byte c,byte d,byte value)   //Функция вывода десятичного числа на газоразрядный индикатор (см.документацию)
{
    digitalWrite(d, (value & 0x08) >> 3);
    digitalWrite(c, (value & 0x04) >> 2);
    digitalWrite(b, (value & 0x02) >> 1);
    digitalWrite(a, value & 0x01);
}

void ShowNumber(byte* numberTime)
{
   
    if((millis()-currentTime)> toflick)
    {

        digitalWrite(NumTransistors[digit], LOW);
        digit++;
        if(digit > 5) digit = 0;
        digitalWrite(NumTransistors[digit], HIGH);
        NixieShow(A,B,C,D,numberTime[digit]);
        currentTime = millis();
    }
    else
    {
        digitalWrite(NumTransistors[digit], HIGH);
    } 
}


void ShowSettingsStay(byte NowDigit,int* SettingsMas){ //функция мигание выбранным разрядом при настройке

   if((millis()-currentTime)> toflick)
    {
        digitalWrite(NumTransistors[digit], LOW);
        digit++;
        if(digit == NowDigit){ //если попадаем на выбранный разряд 
        CountON++; //инкрементируем счётчик,который считает сколько разряд пробыл во включённом состоянии
          if(CountOFF) digit++;
        }
        if(digit > 5) digit = 0;
        digitalWrite(NumTransistors[digit], HIGH);
        NixieShow(A,B,C,D,SettingsMas[digit]);
        currentTime = millis();
    }
    else
    {
        digitalWrite(NumTransistors[digit], HIGH);
    }

    if(CountON == 12 && !CountOFF) { //если разряд пробыл во включённом состоянии 12-счётчик*((3-время горения одного разряда*6-кол-во разрядов)+3-время горения выбранного разряда)=250мс
        digitalWrite(NumTransistors[NowDigit], LOW); //выключим его на тоже время 250мс
        CountOFF = true; //ставим флаг 
        CountON = 0; //обнуляем счётчик
    }
    if(CountON == 12 && CountOFF){
        CountOFF = false;
        CountON=0;
    }
}

void AntiPoisoning(byte* NowTime){ //Функция от отравление катодов ламп

 if(NowTime[3] != previosMinuts){ //каждую минуту включать ф-ию
    previosMinuts = NowTime [3]; //запишем изменение минуты 
    static byte* AnimMas;           //буферный массив для анимации 
    for (int i = 0; i < 4; i++)       //записываем в буфер значения из актуального массива времени
    {
        AnimMas[i] = NowTime [i] + 1; //намеренно изменяем значение времени на 1
    }
    AnimMas[5] = AnimMas[6] = 0;      //секунды заменяем

   while(AnimMas [0] != NowTime[0]){  //пока значения в буфере не станут равным актуальному,значению перебираем цифры
       for (int i = 0; i < 2; i++)  //Цикл чтобы значения на лампах показывались чуть дольше
       {
           ShowNumber(AnimMas);      //Выводим буферный массив на лампы
       }
       AnimMas[0]++;                   //инкрементируем выбранный разряд
       if(AnimMas[0] > 9) AnimMas[0] = 0; //если произошло переполнение зануляем его
       AnimMas [5] = (tm.Second)%10;    //получаем актуальное значение секунд
   }
   while(AnimMas [1] != NowTime[1]){
       for (int i = 0; i < 2; i++)
       {
           ShowNumber(AnimMas);
       }
       AnimMas[1]++;
       if(AnimMas[1] > 9) AnimMas[1] = 0;
       AnimMas [5] = (tm.Second)%10;
   }
   while(AnimMas [2] != NowTime[2]){
       for (int i = 0; i < 2; i++)
       {
           ShowNumber(AnimMas);
       }
       AnimMas[2]++;
       if(AnimMas[2] > 9) AnimMas[2] = 0;
       AnimMas [5] = (tm.Second)%10;
   }
   while(AnimMas [3] != NowTime[3]){
       for (int i = 0; i < 2; i++)
       {
           ShowNumber(AnimMas);
       }
       AnimMas[3]++;
       if(AnimMas[3] > 9) AnimMas[3] = 0;
       AnimMas [5] = (tm.Second)%10;
   }
   GetTimeandData();
 }
}




void GetTimeandData() //Синхронизация отображаемого времени и вренмени модуля часов
{
    NumNixie[0] = (tm.Hour)/10;
    NumNixie[1] = (tm.Hour)%10;
    NumNixie[2] = (tm.Minute)/10;
    NumNixie[3] = (tm.Minute)%10;
    NumNixie[4] = (tm.Second)/10;
    NumNixie[5] = (tm.Second)%10;
    Data[0] = (tm.Day)/10;
    Data[1] = (tm.Day)%10;
    Data[2] = (tm.Month)/10;
    Data[3] = (tm.Month)%10;
    Data[4] = ((tm.Year)%100)/10;
    Data[5] = ((tm.Year)%100)%10;
}

void CheckButtonsMenu ()  {     //РЕАКЦИЯ НА КНОПКИ В РЕЖИМЕ МЕНЮ
 if((MachineState == 3) && (button_up.Loop() == SB_CLICK)){ //если в режиме меню была нажата кнопка вверх
    if(SetDigit == 0){ //если выбранный разряд это десятки часов 
   SettingsNumber[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == 3) SettingsNumber[SetDigit] = 0;
 }
 if(SetDigit == 2 || SetDigit == 4){ //если выбранный разряд это десятки минут или десятки секунд
   SettingsNumber[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == 6) SettingsNumber[SetDigit] = 0;
 }
    SettingsNumber[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == 10) SettingsNumber[SetDigit] = 0;//если произошло переполнение разряда 
 }

 if((MachineState == 3) && (button_down.Loop() == SB_CLICK)){ //если в режиме меню была нажата кнопка вниз
    if(SetDigit == 0){ //если выбранный разряд это десятки часов 
   SettingsNumber[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == -1) SettingsNumber[SetDigit] = 2;
 }
 if(SetDigit == 2 || SetDigit == 4){ //если выбранный разряд это десятки минут или десятки секунд
   SettingsNumber[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == -1) SettingsNumber[SetDigit] = 5;
 }
    SettingsNumber[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsNumber[SetDigit] == -1) SettingsNumber[SetDigit] = 9;//если произошло переполнение разряда 
 }


 if((MachineState == 4) && (button_up.Loop() == SB_CLICK)){ //если в режиме меню даты была нажата кнопка вверх
    if(SetDigit == 0){ //если выбранный разряд это десятки дней
   SettingsData[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsData[SetDigit] == 4) SettingsData[SetDigit] = 0;
 }
 if(SetDigit == 2){ //если выбранный разряд это десятки месяца 
   SettingsData[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsData[SetDigit] == 2) SettingsData[SetDigit] = 0;
 }
 if(SettingsData[2] == 1 && SetDigit == 3){ //если есть в десятке месяца 1 то еденицы месяца могут принять значение 
    SettingsData[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsData[SetDigit] == 3) SettingsData[SetDigit] = 0;
 }
    SettingsData[SetDigit]++; //инкрементируем выбранный разряд
   if(SettingsData[SetDigit] == 10) SettingsData[SetDigit] = 0;//если произошло переполнение разряда 
 } 

 if((MachineState == 4) && (button_down.Loop() == SB_CLICK)){ //если в режиме меню дата была нажата кнопка вниз
    if(SetDigit == 0){ //если выбранный разряд это десятки дней
   SettingsData[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsData[SetDigit] == -1) SettingsData[SetDigit] = 3;
 }
 if(SetDigit == 2){ //если выбранный разряд это десятки месяца
   SettingsData[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsData[SetDigit] == -1) SettingsData[SetDigit] = 1;
 }
 if(SettingsData[2] == 1 && SetDigit == 3){ //если есть в десятке месяца 1 то еденицы месяца могут принять значение 
    SettingsData[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsData[SetDigit] == -1) SettingsData[SetDigit] = 2;
 }
    SettingsData[SetDigit]--; //декрементируем выбранный разряд
   if(SettingsData[SetDigit] == -1) SettingsData[SetDigit] = 9;//если произошло переполнение разряда 
 }
}

void setup()
{
    //конфигурирование портов
    pinMode(A, OUTPUT);
    pinMode(B, OUTPUT);
    pinMode(C, OUTPUT);
    pinMode(D, OUTPUT);
    digitalWrite(A, LOW);
    digitalWrite(B, LOW);
    digitalWrite(C, LOW);
    digitalWrite(D, LOW);
    pinMode(hour1_pin, OUTPUT);
    pinMode(hour2_pin, OUTPUT);
    pinMode(minut1_pin, OUTPUT);
    pinMode(minut2_pin, OUTPUT);
    pinMode(second1_pin, OUTPUT);
    pinMode(second2_pin, OUTPUT);
    digitalWrite(hour1_pin, HIGH);
    digitalWrite(hour2_pin, LOW);
    digitalWrite(minut1_pin, LOW);
    digitalWrite(minut2_pin, LOW);
    digitalWrite(second1_pin, LOW);
    digitalWrite(second2_pin, LOW);
    pinMode(dot_clock, OUTPUT);
    button_menu.begin();
    button_up.begin();
    button_down.begin();
}

void  loop()
{
                                                                       //ПЕРЕХОДЫ ПО МЕНЮ
if((MachineState != 3) && button_menu.Loop() == SB_CLICK) {  //переключение короткими кликами по кнопкам меню для разных отображений 
        MachineState++;
if(MachineState == 3) MachineState = 0;
}

if((button_menu.Loop() == SB_LONG_CLICK) && (MachineState != 3)){ //при долгом нажатии на кнопку меню попадаем в режим настройки
    MachineState = 3; //переход в режим настройки
    SetDigit = 0; //ставим курсор на десятки часов 
 }
   if((MachineState == 3 || MachineState == 4) && (button_menu.Loop() == SB_LONG_CLICK)){ //при долгом нажатии кнопки меню в режиме настройки выходим из последнего в отображение часов 
    MachineState = 0;
 }
   if((MachineState == 3 || MachineState == 4) && (button_menu.Loop() == SB_CLICK)){ //перход по разрядам часов для настройки (0-десятки часов,...5-еденицы секунд)
    SetDigit++;                                                                                                             //(0-десятки дней,...,5-еденицы года)
    if(SetDigit > 5) SetDigit = 0;
 }

 if((MachineState == 3) && (button_up.Loop() == SB_CLICK) && (button_down.Loop() == SB_CLICK)){ //в режиме настройки при одновременно двух зажатых кнопках вверх и вниз попадаем в режим настройки даты
   MachineState = 4;
    SetDigit = 0; //ставим курсор на десятки изменяемого дня 
 }

 if((MachineState == 3 || MachineState == 4) && (button_menu.Loop() == SB_NONE) && (button_up.Loop() == SB_NONE) && (button_down.Loop() == SB_NONE)){ //Если находимся в режиме настройки и никакая кнопка не нажата начать проверку на выход по времени
    if((millis()-LocatingMenu) > settingsTime) MachineState = 0; 
 }
 else LocatingMenu = millis(); //если в режиме настройки нажали какую-либо кнопку обновить время ожидания 


                                   //цикличное переключение режима вывода часы-дата-температура
if ( (MachineState == 0) && (millis()-interval > changeTime)){ 
    MachineState = 1;
    interval = millis();   
}
 if((MachineState == 1) && (millis()-interval > changeData)){
    MachineState = 2;
    interval = millis();
 }

if( (MachineState == 2) && (millis()-interval > changeTemp)){
    MachineState = 0;
    interval = millis();
}
////////////

switch (MachineState) { //что делаем при разных режимах отображения 
    case 0:
      GetTimeandData();
      ShowNumber(NumNixie);
      AntiPoisoning(NumNixie);
      break;
    case 1:
      ShowNumber(Data);
      break;
      case 2:
      //показывать температуру
      break;
      case 3://режим настройки времени
      ShowSettingsStay(SetDigit,SettingsNumber);
      break;
      case 4://режим настройки даты
      ShowSettingsStay(SetDigit,SettingsData);
      break;

    default:
     ShowNumber(NumNixie);
 }
} 