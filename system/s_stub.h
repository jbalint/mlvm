/** $RCSfile: s_stub.h,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: s_stub.h,v 1.9 2003/03/21 13:12:03 gautran Exp $ 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __S_STUB_H__
#define __S_STUB_H__

#ifdef _MLVM_TMPDIR
# define MLVM_TMPDIR _MLVM_TMPDIR
#else
# define MLVM_TMPDIR "/tmp"
#endif

#ifdef _MLVM_DATADIR
# define MLVM_DATADIR _MLVM_DATADIR
#else
# define MLVM_DATADIR "/usr/local/share/mlvm"
#endif


#define MAX_CLI 50
/** Do not change the order of these defines, the context table depands on it. */
#define CONTEXT_INVALID 0
#define CONTEXT_CLOSING 1
#define CONTEXT_L1      2
#define CONTEXT_L2      3
#define CONTEXT_L3      4

#define C_CMD_NULL      0x00000000
#define C_CMD_ADMN      0x41444D4E
#define C_CMD_EXIT      0x45584954
#define C_CMD_HELP      0x48454C50
#define C_CMD_KILL      0x4B494C4C
#define C_CMD_LOAD      0x4C4F4144
#define C_CMD_PDIS      0x50444953
#define C_CMD_PLIC      0x504C4943
#define C_CMD_PRNT      0x50524E54
#define C_CMD_QUIT      0x51554954
#define C_CMD_SLCT      0x534C4354
#define C_CMD_CVER      0x43564552
#define C_CMD_SVER      0x53564552


#define S_STUB_HELP       "440: "
#define S_STUB_SUCCESS    "550: "
#define S_STUB_NEUTRAL    "660: "
#define S_STUB_FAILURE    "770: "

#define S_STUB_GREETING   S_STUB_SUCCESS "        ! ! ! Welcome to the MLVM Server ! ! !\n" \
                          S_STUB_SUCCESS MLVM_NAME " version " MLVM_VERSION ", Copyright (C) 2003 IMAGO LAB.\n"\
                          S_STUB_SUCCESS MLVM_NAME " comes with ABSOLUTELY NO WARRANTY;\n" \
                          S_STUB_SUCCESS "This is free software, and you are welcome to redistribute it\n"\
                          S_STUB_SUCCESS "under certain conditions; type `PLIC' for License details.\n"

#define S_STUB_ERROR      S_STUB_FAILURE "Unrecognize command.\n"

#define HELP_HEADER       S_STUB_HELP    "Welcome to MLVM Server online help screen\n"
#define HELP_FOOTER       S_STUB_HELP    "May the force be with you !\n"
#define HELP_INVOKE       S_STUB_SUCCESS "Invoking the online help...\n"

#define L1_QUIT           S_STUB_SUCCESS "Have a nice day !\n"

#define L2_START          S_STUB_SUCCESS "Entering Administrator level.\n"
#define L2_STOP           S_STUB_SUCCESS "Leaving Administrator level.\n"
#define L2_QUIT           S_STUB_SUCCESS "Have a nice day Sir.\n\n"

#define LOG_HEADER        S_STUB_NEUTRAL "### LOG records begin:\n"
#define LOG_FOOTER        S_STUB_NEUTRAL "### LOG records end.\n"

#define LOAD_HEADER                      ""
#define LOAD_FOOTER                      ""
#define LOAD_SUCCESS      S_STUB_SUCCESS "Imago Queen Sucessfully loaded.\n"
#define LOAD_FAILURE1     S_STUB_FAILURE "Unable to Load Imago source file (check the file).\n"
#define LOAD_FAILURE2     S_STUB_FAILURE "Low in System Ressources (Imago not loaded).\n"
#define LOAD_RESTRICT     S_STUB_FAILURE "Cannot load Imago (kill active Queen first)\n"

#define KILL_HEADER                      ""
#define KILL_FOOTER                      ""
#define KILL_SUCCESS      S_STUB_SUCCESS "Imago Sucessfully killed.\n"
#define KILL_FAILURE1     S_STUB_FAILURE "Invalid Imago.\n"
#define KILL_FAILURE2     S_STUB_FAILURE "Could not kill Imago (may be already dead ?).\n"
#define KILL_L1_RESTRICT  S_STUB_FAILURE "Use LOAD  command first.\n"
#define KILL_L2_RESTRICT  S_STUB_FAILURE "Use the LOAD or SLCT command first.\n"

#define SLCT_FOOTER       S_STUB_SUCCESS "Imago Queen selected.\n"

#define LICENSE_NA        S_STUB_FAILURE "Software License Unavailable.\n"

#define DISCLAIMER_NA     S_STUB_FAILURE "Software Disclaimer Unavailable.\n"

struct c_stub {
  int application;
  short context;
  int fd;
  pthread_mutex_t lock;
};

struct context_cmd {
  union {
    char s_cmd[4];
    int i_cmd;
  } u_cmd;
  const char *desc;
};

int s_stub_init( int );
void s_stub_cleanup();
int s_stub_proc();

#endif
