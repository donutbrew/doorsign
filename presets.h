#pragma once
#include <Arduino.h>

#define NUM_PRESETS 7

String presets[] = {
  "ICON:available|[big]Available[/big]\\n[small]Come on in[/small]",

  "ICON:meeting|[big]In a meeting[/big]\\n[small]Back later[/small]",

  "ICON:no|[big]{{Do not\ndisturb}}[/big]\\n[small]Working[/small]",

  "ICON:out|[big]Out of office[/big]\\n[small]Back later[/small]",

  "ICON:soon|[big]Returning soon[/big]",

  "ICON:remote|[big]Teleworking[/big]\\n[small]Available online[/small]",

  "ICON:cranky|[big]Cranky[/big]\\n[small]Proceed carefully[/small]"
};