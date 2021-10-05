# Aanpassingen Arduino IDE
## 32k flash size
De DSMR api gebruikt redelijk weinig storage. Om zo veel mogelijk over te houden voor de programmatuur en daarmee de update mogelijkheid kan de flash size gereduceerd worden tot 32k (en wellicht wel minder.)

Om dit te doen dienen twee zaken aangepast te worden.

## 1 boards.txt
De boards.txt dient uitgebreid te worden of beter een nieuwe .local variant aan te maken met onderstaande gegevens. Deze boards.local.txt file dient geplaatst te worden in /Users/<user>/Library/Arduino15/packages/esp8266/hardware/esp8266/2.7.4

inhoud boards.local.txt:
```
generic.menu.eesz.1M32=1MB (FS:32KB OTA:~502KB)
generic.menu.eesz.1M32.build.flash_size=1M
generic.menu.eesz.1M32.build.flash_size_bytes=0x100000
generic.menu.eesz.1M32.build.flash_ld=eagle.flash.1m32.ld
generic.menu.eesz.1M32.build.spiffs_pagesize=256
generic.menu.eesz.1M32.upload.maximum_size=991216
generic.menu.eesz.1M32.build.rfcal_addr=0xFC000
generic.menu.eesz.1M32.build.spiffs_start=0xF3000
generic.menu.eesz.1M32.build.spiffs_end=0xFB000
generic.menu.eesz.1M32.build.spiffs_blocksize=4096
```

## 2 nieuwe ld file
Een nieuwe ld definitie file te vinden in: /Users/<user>/Library/Arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/sdk/ld

Onderstaande nieuwe bestand dient toegevoegd te worden met de naam: eagle.flash.1m32.ld

```
/* Flash Split for 1M chips */
/* sketch @0x40200000 (~935KB) (958448B) */
/* empty  @0x402f1ff0 (~4KB) (4112B) */
/* spiffs @0x402F3000 (~32KB) (32768B) */
/* eeprom @0x402FB000 (4KB) */
/* rfcal  @0x402FC000 (4KB) */
/* wifi   @0x402FD000 (12KB) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40201010, len = 0xf1ff0
}

PROVIDE ( _FS_start = 0x402F3000 );
PROVIDE ( _FS_end = 0x402FB000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x1000 );
PROVIDE ( _EEPROM_start = 0x402fb000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x402F3000 );
PROVIDE ( _SPIFFS_end = 0x402FB000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x1000 );

INCLUDE "local.eagle.app.v6.common.ld"
```
