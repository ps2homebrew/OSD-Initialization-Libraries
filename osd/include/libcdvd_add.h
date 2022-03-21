int cdInitAdd(void);
int sceCdAltReadRegionParams(u8 *data, u32 *stat);
int sceCdAltReadPS1BootParam(char *param, u32 *stat);
int sceCdAltBootCertify(const u8 *data);
int sceCdAltRM(char *ModelName, u32 *stat);
int sceCdAltRcBypassCtl(int bypass, u32 *stat); // TODO: Not implemented.
