#include <sdk_version.h>
#include <cellstatus.h>
#include <cell/cell_fs.h>
#include <cell/pad.h>

#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/event.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/memory.h>
#include <sys/ss_get_open_psid.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netex/net.h>
#include <netex/errno.h>



#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <types.h>

#include "common.h"
#include "stdc.h"


SYS_MODULE_INFO(HENPLUGIN, 0, 1, 0);
SYS_MODULE_START(henplugin_start);
SYS_MODULE_STOP(henplugin_stop);
SYS_MODULE_EXIT(henplugin_exit);

#define THREAD_NAME "henplugin_thread"
#define STOP_THREAD_NAME "henplugin_stop_thread"

extern uint32_t vshmain_EB757101(void);        // get running mode flag, 0 = XMB is running
                                               //                        1 = PS3 Game is running
                                               //                        2 = Video Player (DVD/BD) is running
                                               //                        3 = PSX/PSP Emu is running

#define GetCurrentRunningMode vshmain_EB757101 // _ZN3vsh18GetCooperationModeEv	 | vsh::GetCooperationMode(void)
#define IS_ON_XMB		(GetCurrentRunningMode() == 0)
#define IS_INGAME		(GetCurrentRunningMode() != 0)

static sys_ppu_thread_t thread_id=-1;
static int done = 0;

int henplugin_start(uint64_t arg);
int henplugin_stop(void);

extern int vshmain_87BB0001(int param);
int (*vshtask_notify)(int, const char *) = NULL;

int (*vshmain_is_ss_enabled)(void) = NULL;
int (*View_Find)(const char *) = NULL;
void *(*plugin_GetInterface)(int,int) = NULL;

int (*set_SSHT_)(int) = NULL;

int opd[2] = {0, 0};

typedef struct
{
	int (*DoUnk0)(void);  // 1 Parameter: int value 0 - 4
	int (*DoUnk1)(void);  // 0 Parameter: returns an interface
	int (*DoUnk2)(void);  // 0 Parameter: returns an interface
	int (*DoUnk3)(void);  // 0 Parameter: returns an uint[0x14 / 0x24]
	int (*DoUnk4)(void);
	int (*DoUnk5)(void);  // 3 Parameter: list[] {(reload_category game/network/..,reload_category_items game/...), command amount}  - send (sequences of)xmb command(s)
	int (*ExecXMBcommand)(const char *,void *,int); // 3 Parameter: char* (open_list nocheck/...), void * callback(can be 0), 0
	int (*DoUnk7)(void);  // 2 Parameter:
	int (*DoUnk8)(void);  // 3 Parameter:
	int (*DoUnk9)(void);  // 3 Parameter: void *, void *, void *
	int (*DoUnk10)(void); // 2 Parameter: char * , int * out
	int (*DoUnk11)(void); // 3 Parameter: char * query , char * attribute? , uint8 output[]
	int (*DoUnk12)(void); // 1 Parameter: struct
	int (*DoUnk13)(void); // return 0 / 1 Parameter: int 0-9
	int (*DoUnk14)(void); // return 0 / 2 Parameter: int 0-9,
	int (*DoUnk15)(void); // 3 Parameter: int 0-9, ,
	int (*DoUnk16)(void); // nullsub / 3 Parameter: int 0-9, ,
	int (*DoUnk17)(void); // 5 Parameter: int 0-9,
	int (*DoUnk18)(void); // 1 Parameter:
	int (*DoUnk19)(void); // 1 Parameter:
	int (*DoUnk20)(void); // nullsub / PlayIndicate, 2 Parameter: , int value, (0 = show?, 1=update?, -1 = hide) -  (set_playing 0x%x 0x%llx 0x%llx 0x%llx 0x%llx")
	int (*DoUnk21)(void); // nullsub / 1 Parameter: uint * list (simply both parameter from 20/2 and 3rd terminating = -1)
	int (*DoUnk22)(void); // 0 Parameter / 1 Parameter:
	int (*DoUnk23)(void); // -
	int (*DoUnk24)(void); // 0 Parameter:
	int (*DoUnk25)(void); // 0 Parameter:
	int (*DoUnk26)(void); // 2 Parameter: char * (TropViewMode/backup/FaustPreview...) , char * (group/fixed/on...)
	int (*DoUnk27)(void); // 1 Parameter: char *
	int (*DoUnk28)(void); // 2 Parameter: char * (ReloadXil/AvcRoomItem/...), uint8 xml?_parameters[]
	int (*DoUnk29)(void); // 2 Parameter: char * ,
} explore_plugin_interface;

explore_plugin_interface * explore_interface;

static void * getNIDfunc(const char * vsh_module, uint32_t fnid, int offset)
{
	// 0x10000 = ELF
	// 0x10080 = segment 2 start
	// 0x10200 = code start

	uint32_t table = (*(uint32_t*)0x1008C) + 0x984; // vsh table address

	while(((uint32_t)*(uint32_t*)table) != 0)
	{
		uint32_t* export_stru_ptr = (uint32_t*)*(uint32_t*)table; // ptr to export stub, size 2C, "sys_io" usually... Exports:0000000000635BC0 stru_635BC0:    ExportStub_s <0x1C00, 1, 9, 0x39, 0, 0x2000000, aSys_io, ExportFNIDTable_sys_io, ExportStubTable_sys_io>
		const char* lib_name_ptr =  (const char*)*(uint32_t*)((char*)export_stru_ptr + 0x10);
		if(strncmp(vsh_module, lib_name_ptr, strlen(lib_name_ptr)) == 0)
		{
			// we got the proper export struct
			uint32_t lib_fnid_ptr = *(uint32_t*)((char*)export_stru_ptr + 0x14);
			uint32_t lib_func_ptr = *(uint32_t*)((char*)export_stru_ptr + 0x18);
			uint16_t count = *(uint16_t*)((char*)export_stru_ptr + 6); // number of exports
			for(int i = 0; i < count; i++)
			{
				if(fnid == *(uint32_t*)((char*)lib_fnid_ptr + i*4))
				{
					// take address from OPD
					return (void**)*((uint32_t*)(lib_func_ptr) + i) + offset;
				}
			}
		}
		table += 4;
	}
	return 0;
}

static void show_msg(char* msg)
{
	if(!vshtask_notify)
		vshtask_notify = getNIDfunc("vshtask", 0xA02D46E7, 0);
	
	if(!vshtask_notify) return;

	if(strlen(msg) > 200) msg[200] = NULL; // truncate on-screen message
	vshtask_notify(0, msg);

}

static void enable_ingame_screenshot(void)
{
	((int*)getNIDfunc("vshmain",0x981D7E9F,0))[0] -= 0x2C;
}

int sys_map_path(char *old, char *new);
int sys_map_path(char *old, char *new)
{
	system_call_2(35, (uint64_t)(uint32_t)old,(uint64_t)(uint32_t)new);
	return (int)p1;
}

static void reload_xmb(void)
{
	while(!IS_ON_XMB)
	{
		sys_timer_usleep(70000);
	}
	explore_interface->ExecXMBcommand("reload_category game",0,0);
	explore_interface->ExecXMBcommand("reload_category network",0,0);
}

static inline void _sys_ppu_thread_exit(uint64_t val)
{
	system_call_1(41, val);
}

static inline sys_prx_id_t prx_get_module_id_by_address(void *addr)
{
	system_call_1(461, (uint64_t)(uint32_t)addr);
	return (int)p1;
}
#define SC_STOP_PRX_MODULE 				(482)
#define SC_UNLOAD_PRX_MODULE 			(483)
#define SC_COBRA_SYSCALL8 8

static void unload_prx_module(void)
{

	sys_prx_id_t prx = prx_get_module_id_by_address(unload_prx_module);

	{system_call_3(SC_UNLOAD_PRX_MODULE, (uint64_t)prx, 0, NULL);}

}

static void stop_prx_module(void)
{
	sys_prx_id_t prx = prx_get_module_id_by_address(stop_prx_module);
	int *result=NULL;

	{system_call_6(SC_STOP_PRX_MODULE, (uint64_t)prx, 0, NULL, (uint64_t)(uint32_t)result, 0, NULL);}

}

#define SYSCALL8_OPCODE_HEN_REV		0x1339

static void henplugin_thread(uint64_t arg)
{
	View_Find = getNIDfunc("paf", 0xF21655F3, 0);
	plugin_GetInterface = getNIDfunc("paf", 0x23AFB290, 0);
	int view = View_Find("explore_plugin");
	system_call_1(8, SYSCALL8_OPCODE_HEN_REV); uint16_t hen_version = (int)p1;
	char henver[0x30];
	sprintf(henver, "Welcome to PS3HEN %X.%02X", hen_version>>8, (hen_version & 0xFF));
	
	show_msg((char *)henver);
	
	if(view==0)
	{
		view=View_Find("explore_plugin");
		sys_timer_usleep(70000);
	}
	explore_interface = (explore_plugin_interface *)plugin_GetInterface(view, 1);
	
	enable_ingame_screenshot();
//	sys_timer_usleep(70000);
	reload_xmb();
	
	DPRINTF("Exiting main thread!\n");	
	sys_ppu_thread_exit(0);
}

int henplugin_start(uint64_t arg)
{
	sys_ppu_thread_create(&thread_id, henplugin_thread, 0, 3000, 0x4000, SYS_PPU_THREAD_CREATE_JOINABLE, THREAD_NAME);
		
	// Exit thread using directly the syscall and not the user mode library or we will crash
	_sys_ppu_thread_exit(0);	
	return SYS_PRX_RESIDENT;
}

static void henplugin_stop_thread(uint64_t arg)
{
	DPRINTF("henplugin_stop_thread\n");
	done = 1;
	
	DPRINTF("Exiting stop thread!\n");
	sys_ppu_thread_exit(0);
}

static void finalize_module(void)
{
	uint64_t meminfo[5];
	
	sys_prx_id_t prx = prx_get_module_id_by_address(finalize_module);
	
	meminfo[0] = 0x28;
	meminfo[1] = 2;
	meminfo[3] = 0;
	
	system_call_3(482, prx, 0, (uint64_t)(uint32_t)meminfo);		
}

int henplugin_stop(void)
{
	sys_ppu_thread_t t;
	uint64_t exit_code;
	
	sys_ppu_thread_create(&t, henplugin_stop_thread, 0, 0, 0x2000, SYS_PPU_THREAD_CREATE_JOINABLE, STOP_THREAD_NAME);
	sys_ppu_thread_join(t, &exit_code);	
	
	finalize_module();
	_sys_ppu_thread_exit(0);
	return SYS_PRX_STOP_OK;
}

int henplugin_exit(void);
int henplugin_exit(void)
{ //Leave this as is
	return 0;
}