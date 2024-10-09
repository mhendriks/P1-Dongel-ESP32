// minified code based on https://github.com/Gheotic/ESP-HTML-Compressor
//const PROGMEM char UpdateServerIndex[] = {0X3C,0X68,0X74,0X6D,0X6C,0X20,0X63,0X68,0X61,0X72,0X73,0X65,0X74,0X3D,0X55,0X54,0X46,0X2D,0X38,0X3E,0XA,0X3C,0X73,0X74,0X79,0X6C,0X65,0X3E,0X62,0X6F,0X64,0X79,0X7B,0X62,0X61,0X63,0X6B,0X67,0X72,0X6F,0X75,0X6E,0X64,0X2D,0X63,0X6F,0X6C,0X6F,0X72,0X3A,0X23,0X64,0X64,0X64,0X7D,0X3C,0X2F,0X73,0X74,0X79,0X6C,0X65,0X3E,0XA,0X3C,0X62,0X6F,0X64,0X79,0X3E,0XA,0X3C,0X68,0X31,0X3E,0X44,0X53,0X4D,0X52,0X2D,0X6C,0X6F,0X67,0X67,0X65,0X72,0X20,0X46,0X6C,0X61,0X73,0X68,0X20,0X75,0X74,0X69,0X6C,0X69,0X74,0X79,0X3C,0X2F,0X68,0X31,0X3E,0XA,0X3C,0X66,0X6F,0X72,0X6D,0X20,0X6D,0X65,0X74,0X68,0X6F,0X64,0X3D,0X50,0X4F,0X53,0X54,0X20,0X61,0X63,0X74,0X69,0X6F,0X6E,0X3D,0X22,0X3F,0X63,0X6D,0X64,0X3D,0X30,0X22,0X20,0X65,0X6E,0X63,0X74,0X79,0X70,0X65,0X3D,0X6D,0X75,0X6C,0X74,0X69,0X70,0X61,0X72,0X74,0X2F,0X66,0X6F,0X72,0X6D,0X2D,0X64,0X61,0X74,0X61,0X3E,0XA,0X53,0X65,0X6C,0X65,0X63,0X74,0X65,0X65,0X72,0X20,0X65,0X65,0X6E,0X20,0X22,0X3C,0X62,0X3E,0X2E,0X69,0X6E,0X6F,0X2E,0X62,0X69,0X6E,0X3C,0X2F,0X62,0X3E,0X22,0X20,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X69,0X6E,0X70,0X75,0X74,0X20,0X74,0X79,0X70,0X65,0X3D,0X66,0X69,0X6C,0X65,0X20,0X61,0X63,0X63,0X65,0X70,0X74,0X3D,0X69,0X6E,0X6F,0X2E,0X62,0X69,0X6E,0X20,0X6E,0X61,0X6D,0X65,0X3D,0X66,0X69,0X72,0X6D,0X77,0X61,0X72,0X65,0X3E,0XA,0X3C,0X69,0X6E,0X70,0X75,0X74,0X20,0X74,0X79,0X70,0X65,0X3D,0X73,0X75,0X62,0X6D,0X69,0X74,0X20,0X76,0X61,0X6C,0X75,0X65,0X3D,0X22,0X46,0X6C,0X61,0X73,0X68,0X20,0X46,0X69,0X72,0X6D,0X77,0X61,0X72,0X65,0X22,0X3E,0XA,0X3C,0X2F,0X66,0X6F,0X72,0X6D,0X3E,0XA,0X3C,0X66,0X6F,0X72,0X6D,0X20,0X6D,0X65,0X74,0X68,0X6F,0X64,0X3D,0X50,0X4F,0X53,0X54,0X20,0X61,0X63,0X74,0X69,0X6F,0X6E,0X3D,0X22,0X3F,0X63,0X6D,0X64,0X3D,0X31,0X30,0X30,0X22,0X20,0X65,0X6E,0X63,0X74,0X79,0X70,0X65,0X3D,0X6D,0X75,0X6C,0X74,0X69,0X70,0X61,0X72,0X74,0X2F,0X66,0X6F,0X72,0X6D,0X2D,0X64,0X61,0X74,0X61,0X3E,0XA,0X53,0X65,0X6C,0X65,0X63,0X74,0X65,0X65,0X72,0X20,0X65,0X65,0X6E,0X20,0X22,0X3C,0X62,0X3E,0X2E,0X73,0X70,0X69,0X66,0X66,0X73,0X2E,0X62,0X69,0X6E,0X3C,0X2F,0X62,0X3E,0X22,0X20,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X69,0X6E,0X70,0X75,0X74,0X20,0X74,0X79,0X70,0X65,0X3D,0X66,0X69,0X6C,0X65,0X20,0X61,0X63,0X63,0X65,0X70,0X74,0X3D,0X73,0X70,0X69,0X66,0X66,0X73,0X2E,0X62,0X69,0X6E,0X20,0X6E,0X61,0X6D,0X65,0X3D,0X66,0X69,0X6C,0X65,0X73,0X79,0X73,0X74,0X65,0X6D,0X3E,0XA,0X3C,0X69,0X6E,0X70,0X75,0X74,0X20,0X74,0X79,0X70,0X65,0X3D,0X73,0X75,0X62,0X6D,0X69,0X74,0X20,0X76,0X61,0X6C,0X75,0X65,0X3D,0X22,0X46,0X6C,0X61,0X73,0X68,0X20,0X53,0X70,0X69,0X66,0X66,0X73,0X22,0X3E,0XA,0X3C,0X2F,0X66,0X6F,0X72,0X6D,0X3E,0XA,0X3C,0X68,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0X3C,0X66,0X6F,0X6E,0X74,0X20,0X63,0X6F,0X6C,0X6F,0X72,0X3D,0X72,0X65,0X64,0X3E,0X4C,0X45,0X54,0X20,0X4F,0X50,0X21,0X21,0X21,0X3C,0X2F,0X66,0X6F,0X6E,0X74,0X3E,0XA,0X3C,0X62,0X72,0X3E,0X42,0X69,0X6A,0X20,0X68,0X65,0X74,0X20,0X66,0X6C,0X61,0X73,0X68,0X65,0X6E,0X20,0X76,0X61,0X6E,0X20,0X53,0X50,0X49,0X46,0X46,0X53,0X20,0X72,0X61,0X61,0X6B,0X74,0X20,0X75,0X20,0X64,0X65,0X20,0X52,0X49,0X4E,0X47,0X2D,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X65,0X6E,0X20,0X6B,0X77,0X69,0X6A,0X74,0X2E,0XA,0X3C,0X62,0X72,0X3E,0X4D,0X61,0X61,0X6B,0X20,0X64,0X61,0X61,0X72,0X6F,0X6D,0X20,0X65,0X65,0X72,0X73,0X74,0X20,0X65,0X65,0X6E,0X20,0X6B,0X6F,0X70,0X69,0X65,0X20,0X76,0X61,0X6E,0X20,0X64,0X65,0X7A,0X65,0X20,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X65,0X6E,0X20,0X28,0X76,0X69,0X61,0X20,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X73,0X62,0X65,0X68,0X65,0X65,0X72,0X29,0X20,0X65,0X6E,0X20,0X7A,0X65,0X74,0X20,0X64,0X65,0X20,0X64,0X61,0X74,0X61,0X2D,0X62,0X65,0X73,0X74,0X61,0X6E,0X64,0X65,0X6E,0X20,0X6E,0X61,0X20,0X68,0X65,0X74,0X20,0X66,0X6C,0X61,0X73,0X68,0X65,0X6E,0X20,0X76,0X61,0X6E,0X20,0X53,0X50,0X49,0X46,0X46,0X53,0X20,0X77,0X65,0X65,0X72,0X20,0X74,0X65,0X72,0X75,0X67,0X2E,0XA,0X3C,0X68,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0X57,0X61,0X63,0X68,0X74,0X20,0X6E,0X6F,0X67,0X20,0X3C,0X73,0X70,0X61,0X6E,0X20,0X73,0X74,0X79,0X6C,0X65,0X3D,0X66,0X6F,0X6E,0X74,0X2D,0X73,0X69,0X7A,0X65,0X3A,0X31,0X2E,0X33,0X65,0X6D,0X20,0X69,0X64,0X3D,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X3E,0X31,0X32,0X30,0X3C,0X2F,0X73,0X70,0X61,0X6E,0X3E,0X20,0X73,0X65,0X63,0X6F,0X6E,0X64,0X65,0X6E,0X20,0X2E,0X2E,0XA,0X3C,0X62,0X72,0X3E,0X41,0X6C,0X73,0X20,0X68,0X65,0X74,0X20,0X6C,0X69,0X6A,0X6B,0X74,0X20,0X6F,0X66,0X20,0X65,0X72,0X20,0X6E,0X69,0X65,0X74,0X73,0X20,0X67,0X65,0X62,0X65,0X75,0X72,0X64,0X2C,0X20,0X77,0X61,0X63,0X68,0X74,0X20,0X64,0X61,0X6E,0X20,0X74,0X6F,0X74,0X20,0X64,0X65,0X20,0X74,0X65,0X6C,0X6C,0X65,0X72,0X20,0X6F,0X70,0X20,0X27,0X6E,0X75,0X6C,0X27,0X20,0X73,0X74,0X61,0X61,0X74,0X20,0X65,0X6E,0X20,0X6B,0X6C,0X69,0X6B,0X20,0X64,0X61,0X61,0X72,0X6E,0X61,0X20,0X3C,0X73,0X70,0X61,0X6E,0X20,0X73,0X74,0X79,0X6C,0X65,0X3D,0X66,0X6F,0X6E,0X74,0X2D,0X73,0X69,0X7A,0X65,0X3A,0X31,0X2E,0X33,0X65,0X6D,0X3E,0X3C,0X62,0X3E,0X3C,0X61,0X20,0X68,0X72,0X65,0X66,0X3D,0X2F,0X20,0X3E,0X68,0X69,0X65,0X72,0X3C,0X2F,0X61,0X3E,0X3C,0X2F,0X62,0X3E,0X3C,0X2F,0X73,0X70,0X61,0X6E,0X3E,0X21,0XA,0X3C,0X2F,0X62,0X6F,0X64,0X79,0X3E,0XA,0X3C,0X73,0X63,0X72,0X69,0X70,0X74,0X3E,0X76,0X61,0X72,0X20,0X73,0X3D,0X64,0X6F,0X63,0X75,0X6D,0X65,0X6E,0X74,0X2E,0X67,0X65,0X74,0X45,0X6C,0X65,0X6D,0X65,0X6E,0X74,0X42,0X79,0X49,0X64,0X28,0X22,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X22,0X29,0X2E,0X74,0X65,0X78,0X74,0X43,0X6F,0X6E,0X74,0X65,0X6E,0X74,0X2C,0X63,0X6F,0X75,0X6E,0X74,0X64,0X6F,0X77,0X6E,0X3D,0X73,0X65,0X74,0X49,0X6E,0X74,0X65,0X72,0X76,0X61,0X6C,0X28,0X66,0X75,0X6E,0X63,0X74,0X69,0X6F,0X6E,0X28,0X29,0X7B,0X73,0X2D,0X2D,0X2C,0X28,0X64,0X6F,0X63,0X75,0X6D,0X65,0X6E,0X74,0X2E,0X67,0X65,0X74,0X45,0X6C,0X65,0X6D,0X65,0X6E,0X74,0X42,0X79,0X49,0X64,0X28,0X22,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X22,0X29,0X2E,0X74,0X65,0X78,0X74,0X43,0X6F,0X6E,0X74,0X65,0X6E,0X74,0X3D,0X73,0X29,0X3C,0X3D,0X30,0X26,0X26,0X28,0X63,0X6C,0X65,0X61,0X72,0X49,0X6E,0X74,0X65,0X72,0X76,0X61,0X6C,0X28,0X63,0X6F,0X75,0X6E,0X74,0X64,0X6F,0X77,0X6E,0X29,0X2C,0X77,0X69,0X6E,0X64,0X6F,0X77,0X2E,0X6C,0X6F,0X63,0X61,0X74,0X69,0X6F,0X6E,0X2E,0X61,0X73,0X73,0X69,0X67,0X6E,0X28,0X22,0X2F,0X22,0X29,0X29,0X7D,0X2C,0X31,0X65,0X33,0X29,0X3C,0X2F,0X73,0X63,0X72,0X69,0X70,0X74,0X3E,0XA,0X3C,0X2F,0X68,0X74,0X6D,0X6C,0X3E};
//const PROGMEM char UpdateServerSuccess[] = {0X3C,0X68,0X74,0X6D,0X6C,0X20,0X63,0X68,0X61,0X72,0X73,0X65,0X74,0X3D,0X55,0X54,0X46,0X2D,0X38,0X3E,0XA,0X3C,0X73,0X74,0X79,0X6C,0X65,0X3E,0X62,0X6F,0X64,0X79,0X7B,0X62,0X61,0X63,0X6B,0X67,0X72,0X6F,0X75,0X6E,0X64,0X2D,0X63,0X6F,0X6C,0X6F,0X72,0X3A,0X23,0X64,0X64,0X64,0X7D,0X3C,0X2F,0X73,0X74,0X79,0X6C,0X65,0X3E,0XA,0X3C,0X62,0X6F,0X64,0X79,0X3E,0XA,0X3C,0X68,0X31,0X3E,0X44,0X53,0X4D,0X52,0X2D,0X6C,0X6F,0X67,0X67,0X65,0X72,0X20,0X46,0X6C,0X61,0X73,0X68,0X20,0X75,0X74,0X69,0X6C,0X69,0X74,0X79,0X3C,0X2F,0X68,0X31,0X3E,0XA,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X68,0X32,0X3E,0X55,0X70,0X64,0X61,0X74,0X65,0X20,0X73,0X75,0X63,0X63,0X65,0X73,0X73,0X66,0X75,0X6C,0X21,0X3C,0X2F,0X68,0X32,0X3E,0XA,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0X57,0X61,0X69,0X74,0X20,0X66,0X6F,0X72,0X20,0X74,0X68,0X65,0X20,0X44,0X53,0X4D,0X52,0X2D,0X6C,0X6F,0X67,0X67,0X65,0X72,0X20,0X74,0X6F,0X20,0X72,0X65,0X62,0X6F,0X6F,0X74,0X20,0X61,0X6E,0X64,0X20,0X73,0X74,0X61,0X72,0X74,0X20,0X74,0X68,0X65,0X20,0X48,0X54,0X54,0X50,0X20,0X73,0X65,0X72,0X76,0X65,0X72,0XA,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0XA,0X3C,0X62,0X72,0X3E,0X57,0X61,0X63,0X68,0X74,0X20,0X6E,0X6F,0X67,0X20,0X3C,0X73,0X70,0X61,0X6E,0X20,0X73,0X74,0X79,0X6C,0X65,0X3D,0X66,0X6F,0X6E,0X74,0X2D,0X73,0X69,0X7A,0X65,0X3A,0X31,0X2E,0X33,0X65,0X6D,0X20,0X69,0X64,0X3D,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X3E,0X36,0X30,0X3C,0X2F,0X73,0X70,0X61,0X6E,0X3E,0X20,0X73,0X65,0X63,0X6F,0X6E,0X64,0X65,0X6E,0X20,0X2E,0X2E,0XA,0X3C,0X62,0X72,0X3E,0X41,0X6C,0X73,0X20,0X68,0X65,0X74,0X20,0X6C,0X69,0X6A,0X6B,0X74,0X20,0X6F,0X66,0X20,0X65,0X72,0X20,0X6E,0X69,0X65,0X74,0X73,0X20,0X67,0X65,0X62,0X65,0X75,0X72,0X64,0X2C,0X20,0X77,0X61,0X63,0X68,0X74,0X20,0X64,0X61,0X6E,0X20,0X74,0X6F,0X74,0X20,0X64,0X65,0X20,0X74,0X65,0X6C,0X6C,0X65,0X72,0X20,0X6F,0X70,0X20,0X27,0X6E,0X75,0X6C,0X27,0X20,0X73,0X74,0X61,0X61,0X74,0X20,0X65,0X6E,0X20,0X6B,0X6C,0X69,0X6B,0X20,0X64,0X61,0X61,0X72,0X6E,0X61,0X20,0X3C,0X73,0X70,0X61,0X6E,0X20,0X73,0X74,0X79,0X6C,0X65,0X3D,0X66,0X6F,0X6E,0X74,0X2D,0X73,0X69,0X7A,0X65,0X3A,0X31,0X2E,0X33,0X65,0X6D,0X3E,0X3C,0X62,0X3E,0X3C,0X61,0X20,0X68,0X72,0X65,0X66,0X3D,0X2F,0X20,0X3E,0X68,0X69,0X65,0X72,0X3C,0X2F,0X61,0X3E,0X3C,0X2F,0X62,0X3E,0X3C,0X2F,0X73,0X70,0X61,0X6E,0X3E,0X21,0XA,0X3C,0X2F,0X62,0X6F,0X64,0X79,0X3E,0XA,0X3C,0X73,0X63,0X72,0X69,0X70,0X74,0X3E,0X76,0X61,0X72,0X20,0X73,0X3D,0X64,0X6F,0X63,0X75,0X6D,0X65,0X6E,0X74,0X2E,0X67,0X65,0X74,0X45,0X6C,0X65,0X6D,0X65,0X6E,0X74,0X42,0X79,0X49,0X64,0X28,0X22,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X22,0X29,0X2E,0X74,0X65,0X78,0X74,0X43,0X6F,0X6E,0X74,0X65,0X6E,0X74,0X2C,0X63,0X6F,0X75,0X6E,0X74,0X64,0X6F,0X77,0X6E,0X3D,0X73,0X65,0X74,0X49,0X6E,0X74,0X65,0X72,0X76,0X61,0X6C,0X28,0X66,0X75,0X6E,0X63,0X74,0X69,0X6F,0X6E,0X28,0X29,0X7B,0X73,0X2D,0X2D,0X2C,0X28,0X64,0X6F,0X63,0X75,0X6D,0X65,0X6E,0X74,0X2E,0X67,0X65,0X74,0X45,0X6C,0X65,0X6D,0X65,0X6E,0X74,0X42,0X79,0X49,0X64,0X28,0X22,0X77,0X61,0X69,0X74,0X53,0X65,0X63,0X6F,0X6E,0X64,0X73,0X22,0X29,0X2E,0X74,0X65,0X78,0X74,0X43,0X6F,0X6E,0X74,0X65,0X6E,0X74,0X3D,0X73,0X29,0X3C,0X3D,0X30,0X26,0X26,0X28,0X63,0X6C,0X65,0X61,0X72,0X49,0X6E,0X74,0X65,0X72,0X76,0X61,0X6C,0X28,0X63,0X6F,0X75,0X6E,0X74,0X64,0X6F,0X77,0X6E,0X29,0X2C,0X77,0X69,0X6E,0X64,0X6F,0X77,0X2E,0X6C,0X6F,0X63,0X61,0X74,0X69,0X6F,0X6E,0X2E,0X61,0X73,0X73,0X69,0X67,0X6E,0X28,0X22,0X2F,0X22,0X29,0X29,0X7D,0X2C,0X31,0X65,0X33,0X29,0X3C,0X2F,0X73,0X63,0X72,0X69,0X70,0X74,0X3E,0XA,0X3C,0X2F,0X68,0X74,0X6D,0X6C,0X3E};

/*
 * Server Index Page
 * 
 * 
 */

//static const char UpdateHTML[] PROGMEM = "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script><form method='POST' action='#' enctype='multipart/form-data' id='upload_form'><h1>Update P1 Dongel</h1><br><div>Update software of data van de P1 Dongel. Software is te herkennen aan <b>.bin(.gz)</b> databestand aan <b>.spiffs.bin(.gz)</b><br><br></div><input type='file' name='update' id='file' onchange='sub(this)'><label id='file-input' for='file'><span>Selecteer file...</span></label><div id='prg'></div><div id='prgbar'><div id='bar'></div></div><br><input type='submit' class=btn value='UPDATE' onchange='upload(this)'></div><div id=letop>LETOP!<br>Maak een kopie van databestanden (via bestandsbeheer) en zet de databestanden na het updaten weer terug.</div></form><script>function sub(obj){var fileName=obj.value.split('\\\');$('#file-input').html('<span>'+ fileName[fileName.length-1]+'</span>');if (obj.value.indexOf('spiffs')>-1){$('#letop').css('display','block')}else $('#letop').css('display','none');}$('form').submit(function(e){e.preventDefault();var form=$('#upload_form')[0];var data=new FormData(form);$.ajax({url: '/update',type: 'POST',data: data,contentType: false,processData:false,xhr: function(){var xhr=new window.XMLHttpRequest();xhr.upload.addEventListener('progress', function(e){if (e.lengthComputable){var per=100 * e.loaded / e.total;$('#prg').html('voortgang: '+per.toFixed(0)+'%');$('#bar').css('width',per.toFixed(0)+'%');}}, false);xhr.upload.addEventListener('load', function(e){$('#prg').html('Uploaded : herstarten ...');}, false);return xhr;},success:function(d, s){console.log('success!');document.location.href='/';},error: function (a, b, c){}});});</script><style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}#file-input{padding:0;border:1px solid #ddd;line-height:44px;display:block;cursor:pointer}#bar,#prgbar{background-color:#f1f1f1;border-radius:10px;width:100%}#bar{background-color:#3498db;width:0%;height:10px}#file-input span{margin-left:6px}form{background:#fff;max-width:450px;margin:75px auto;padding:30px;border-radius:5px}.btn{background:#3498db;color:#fff;cursor:pointer}div{margin:10px auto}h1{text-align:center}#letop, input[type=file]{display:none;color:red}</style>";
//<script src='jquery.min.js'></script> -> AP mode

static const char UpdateHTML[] PROGMEM =
R"(<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>
<form method='POST' action='#' enctype='multipart/form-data' id='uf'>
<h1>Dongle Update</h1><br>
<div>Update software of data van de Dongel. Software is te herkennen aan <b>.bin(.gz)</b> databestand aan <b>.spiffs.bin(.gz)</b><br><br></div>
<input type='file' name='update' id='f' onchange='sub(this)'>
<label id='fi' for='f'><span>Selecteer file...</span></label>
<div id='prg'></div><div id='pb'><div id='bar'></div></div><br>
<input type='submit' id='xx' class=btn value='UPDATE' onchange='upload(this)'></div>
<div id=w>LETOP!<br>Maak een kopie van databestanden (via bestandsbeheer).</div>
</form>
<script>
function sub(obj){
var fileName = obj.value.split('\\\\');
$('#fi').html('<span>'+ fileName[fileName.length-1]+'</span>');
if (obj.value.indexOf('spiffs')>-1) {$('#w').css('display','block')} else $('#w').css('display','none');}
$('form').submit(function(e){
if(!$("form input[type=file]").val()) {
 alert('You must select a file!');
 return false;
}
e.preventDefault();
var form = $('#uf')[0];
var data = new FormData(form);
$.ajax({
url: '/update',
type: 'POST',
data: data,
contentType: false,
processData:false,
xhr: function() {
var xhr = new window.XMLHttpRequest();
xhr.upload.addEventListener('progress', function(e) {
if (e.lengthComputable) {
var per = 100 * e.loaded / e.total;
$('#prg').html('voortgang: '+per.toFixed(0)+'%');
$('#bar').css('width',per.toFixed(0)+'%');
}
}, false);
xhr.upload.addEventListener('load', function(e) {
$('#prg').html('Uploaded : herstarten ...');
}, false);
return xhr;
},
success:function(d, s) {
console.log('success!');
document.location.href='/';
},
error: function (a, b, c) {
}
});
});
</script><style>
#fi,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}
input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}
#fi{padding:0;border:1px solid #ddd;line-height:44px;display:block;cursor:pointer}
#bar,#pb{background-color:#f1f1f1;border-radius:10px;width:100%}#bar{background-color:#3498db;width:0%;height:10px}
#fi span{margin-left:6px}
form{background:#fff;max-width:450px;margin:75px auto;padding:30px;border-radius:5px}
.btn{background:#3498db;color:#fff;cursor:pointer}
div{margin:10px auto}
h1{text-align:center}
#w, input[type=file]{display:none;color:red}
</style>)"; 