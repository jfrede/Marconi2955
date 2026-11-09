/* Links the current firmware modules for the 8085 so that their code size can be measured. */
#include "../app.h"
#include "../keys.h"

int main(void)
{
    app_init();
    for (;;)
        app_key(keys_scan());
}
