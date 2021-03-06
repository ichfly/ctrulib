#ifndef CSND_H
#define CSND_H

#define CSND_SHAREDMEM_DEFAULT 0x10004000

//See here regarding CSND shared-mem commands, etc: http://3dbrew.org/wiki/CSND_Shared_Memory

Result CSND_initialize(u32* sharedMem);
Result CSND_shutdown();

Result CSND_playsound(u32 channel, u32 looping, u32 encoding, u32 samplerate, u32 *vaddr0, u32 *vaddr1, u32 totalbytesize, u32 unk0, u32 unk1);
void CSND_setchannel_playbackstate(u32 channel, u32 value);
void CSND_sharedmemtype0_cmd0(u32 channel, u32 value);
void CSND_writesharedmem_cmdtype0(u16 cmdid, u8 *cmdparams);
Result CSND_sharedmemtype0_cmdupdatestate(int waitdone);

Result CSND_getchannelstate(u32 entryindex, u32 *out);
Result CSND_getchannelstate_isplaying(u32 entryindex, u8 *status);

#endif

