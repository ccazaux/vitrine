/*
   Vitrine tactile
   Carte utilisée : ESP32S3-USB-OTG avec capacité a gérer touch + USB keyboard
   Auteur : CCA pour ESAAIX
*/

#include "USB.h"
#include "USBHIDKeyboard.h"
USBHIDKeyboard Keyboard;

#define DBGBUTTON 0
// Touchs PINS are  GPIO 1 -14 matching to TOUCH 1 14
// Les broches 1  4 7  8 10  14 ont été choisies car elles sont espacées, 
// de façon à éviter le crosstalk sur des antennes trop proches entre elles.

#define PIN_UP  1   //UP 
#define PIN_JMP 4   //JMP
#define PIN_LFT 7   //LFT
#define PIN_DN  8   //DN
#define PIN_ALT 10  //ALT (mais non utilisé)
#define PIN_RGT 13  //RIGHT

//Sorties Leds de debug ; voisines des entrees touch testees
#define PINLED_UP  GPIO_NUM_2   
#define PINLED_JMP GPIO_NUM_5   
#define PINLED_LFT GPIO_NUM_15  
#define PINLED_DN  GPIO_NUM_3   
#define PINLED_ALT GPIO_NUM_11  
#define PINLED_RGT GPIO_NUM_14  

//Structure associée à chaque entrée capacitive / touche du gamepad tactile.
typedef struct {
  int         touchPin;         // Numéro de broche capacitivé
  gpio_num_t  touchLED;         // Numéro de LED de debug
  int32_t     oldTouch   ;      // Valeur lue sur l'entrée capacitive à l'instant t-1
  int32_t     curTouch   ;      // Valeur lue sur l'entrée capacitive à l'instant t
  int32_t     deltaTouch ;      // Différence entre la valeur lue aux instants t et t-1
  int32_t     dynamicThreshold ;// Seuillage dynamique
  int32_t     staticThreshold  ;// Seuillage statique
  int         state ;           // Etat courant du bouton
  int         stateOld ;        // Etat précédant du bouton
} capa;

//chaine de caracteres pour debug
char     sdbg[128] = {0};

//Parametres validés avec vitrine de test ESA 240126
/*
  capa c_up  = {PIN_UP , PINLED_UP , 0x7FFFFFFF, 0, 0, 400, 70000, 0, 0};
  capa c_dn  = {PIN_DN , PINLED_DN , 0x7FFFFFFF, 0, 0, 400, 65000, 0, 0};
  capa c_lft = {PIN_LFT, PINLED_LFT, 0x7FFFFFFF, 0, 0, 400, 65000, 0, 0};
  capa c_rgt = {PIN_RGT, PINLED_RGT, 0x7FFFFFFF, 0, 0, 400, 65000, 0, 0};
  capa c_jmp = {PIN_JMP, PINLED_JMP, 0x7FFFFFFF, 0, 0, 400, 60000, 0, 0};
  capa c_alt = {PIN_ALT, PINLED_ALT, 0x7FFFFFFF, 0, 0, 400, 65000, 0, 0};
*/

//Parametres validés avec vitrine DigitalZone
capa c_up  = {PIN_UP , PINLED_UP , 0x7FFFFFFF, 0, 0, 4000, 50000, 0, 0};
capa c_dn  = {PIN_DN , PINLED_DN , 0x7FFFFFFF, 0, 0, 4000, 50000, 0, 0};
capa c_lft = {PIN_LFT, PINLED_LFT, 0x7FFFFFFF, 0, 0, 4000, 50000, 0, 0};
capa c_rgt = {PIN_RGT, PINLED_RGT, 0x7FFFFFFF, 0, 0, 4000, 45000, 0, 0};
capa c_jmp = {PIN_JMP, PINLED_JMP, 0x7FFFFFFF, 0, 0, 4000, 40000, 0, 0};
capa c_alt = {PIN_ALT, PINLED_ALT, 0x7FFFFFFF, 0, 0, 4000, 65000, 0, 0};


void setup()
{
  //Sorties LEDS DE DEBUG
  gpio_set_direction(PINLED_UP, GPIO_MODE_OUTPUT);
  gpio_set_direction(PINLED_DN, GPIO_MODE_OUTPUT);
  gpio_set_direction(PINLED_LFT, GPIO_MODE_OUTPUT);
  gpio_set_direction(PINLED_RGT, GPIO_MODE_OUTPUT);
  gpio_set_direction(PINLED_JMP, GPIO_MODE_OUTPUT);
  gpio_set_direction(PINLED_ALT, GPIO_MODE_OUTPUT);

  //masses virtuelles pour LEDS de DEBUG)
  gpio_set_direction(GPIO_NUM_6, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_6, 0);

  gpio_set_direction(GPIO_NUM_16, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_16, 0);

  gpio_set_direction(GPIO_NUM_46, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_46, 0);

  gpio_set_direction(GPIO_NUM_12, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_12, 0);

  gpio_set_direction(GPIO_NUM_42, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_42, 0);

  // Initialiasation liaison série : le baud rate à 1Mbits/sec permet de limiter
  // l'impact de ces informations de debug sur la réactivité du systèle
  Serial.begin(1000000);
  Serial.println("Starting USB Keyboard ESP32-S3!");

  //Initialisation Clavier USB
  Keyboard.begin();
  USB.begin();
}

// Gestion d'un appui touche : on envoie juste les commandes pout indiquer
// qu'on a appuyé ou relaché une touche, pour ne pas encombrer la liaison USB ou Bluetooth
// et obtenir un fonctionnement le plus semblable possible à un clavier / gamepad.
// Fonction générique pour toutes les touches
void manageKey(capa * cap, uint8_t KEY)
{
  //Test appuyé ou relaché
  cap->curTouch = touchRead(cap->touchPin);
  cap->deltaTouch = cap->curTouch - cap->oldTouch;

  sprintf(sdbg, "%06d,", cap->curTouch);   Serial.print(sdbg);
  sprintf(sdbg, "%06d,", cap->deltaTouch); Serial.print(sdbg);
  cap->oldTouch = cap->curTouch;

  //if ( ( cap->deltaTouch > cap->dynamicThreshold) && (cap->touchPressed == false) ) //Seuil dynamique desactivé : le seuil statique est suffisant.
  if ( //( cap->deltaTouch > cap->dynamicThreshold)  ||
    ( cap->curTouch > cap->staticThreshold)
  )
  {
    /*Serial.print("PRESS!");
      sprintf(sdbg, "del= %d", cap->deltaTouch); Serial.print(sdbg);
      sprintf(sdbg, "cur= %d", cap->curTouch);   Serial.print(sdbg);
      Serial.println();*/
    cap->state = true;
  }
  else //if ( (cap->deltaTouch < -cap->dynamicThreshold)  )
  {
    /*Serial.print("RELEASE!");
      sprintf(sdbg, "del= %d", cap->deltaTouch); Serial.print(sdbg);
      sprintf(sdbg, "cur= %d", cap->curTouch);   Serial.print(sdbg);
      Serial.println();*/
    cap->state = false;
  }

  if (  cap->stateOld != cap->state)
  {
#if DBGBUTTON
    Serial.print(cap->touchPin);
    Serial.print("chgto");
    Serial.println(cap->state);
#endif
    if (cap->state == HIGH)
    {
      Keyboard.press(KEY);
      gpio_set_level(cap->touchLED, 1);//
    }
    else
    {
      Keyboard.release(KEY);
      gpio_set_level(cap->touchLED, 0);//
    }
    cap->stateOld = cap->state;
  }
}

//Boucle principale : Chaque touche est gérée via sa structure et on lui fait correspondre une touche clavier
void loop()
{
  manageKey( &c_up , 'w' );
  manageKey( &c_dn , 's' );
  manageKey( &c_lft, 'a' );
  manageKey( &c_rgt, 'd' );
  manageKey( &c_jmp, ' ' );
  manageKey( &c_alt, 'p' );

  Serial.println("0");
  delay(50);
}
