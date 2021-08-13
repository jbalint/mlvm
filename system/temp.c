/** $RCSfile: temp.c,v $
 * Copyright (C) 2003  X. Li <xli@draco.cis.uoguelph.ca>,
 *                     G. Autran <gautran@draco.cis.uoguelph.ca>,
 *                     H. Liang <hliang@draco.cis.uoguelph.ca>
 * $Id: temp.c,v 1.2 2003/03/21 13:12:03 gautran Exp $ 
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
#define adjust_code(ITYPE, IDIRECT, IADJUST, IHASH) {             \\
	switch (ITYPE) {                                          \\
          caseop(I_STATIONARY):                                   \\
          caseop(I_WORKER):                                       \\
          caseop(I_MESSENGER):                                    \\
          caseop(I_E):                                            \\
		IDIRECT;                                          \\
		IADJUST;                                          \\
		break;                                            \\
	  caseop(I_O):                                            \\
 	  	IDIRECT;                                          \\
	  	break;                                            \\
          caseop(I_N):                                            \\
          caseop(I_C):                                            \\
          caseop(I_F):                                            \\
          caseop(I_B):                                            \\
          caseop(I_I):                                            \\
          caseop(I_P):                                            \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		break;                                            \\
          caseop(I_R):                                            \\
          caseop(I_2N):                                           \\
          caseop(I_NC):                                           \\
          caseop(I_NF):                                           \\
          caseop(I_NI):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
  		break;                                            \\
          caseop(I_3N):                                           \\
          caseop(I_NR):                                           \\
          caseop(I_2NF):                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
   		break;                                            \\
          caseop(I_4N):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
   		break;                                            \\
          caseop(I_5N):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
  		break;                                            \\
          caseop(I_6N):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
  		break;                                            \\
          caseop(I_7N):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
  		break;                                            \\
          caseop(I_8N):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
   		break;                                            \\
          caseop(I_NE):                                           \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IADJUST;                                          \\
		break;                                            \\
          caseop(I_2NE):                                          \\
          caseop(I_NFE):                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IADJUST;                                          \\
		break;                                            \\
          caseop(I_2NFE):                                         \\
          caseop(I_3NE):                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IADJUST;                                          \\
		break;                                            \\
          caseop(I_3NFE):                                         \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
 		IADJUST;                                          \\
		break;                                            \\
          caseop(I_4E):                                           \\
		IDIRECT;                                          \\
		IADJUST;                                          \\
		IADJUST;                                          \\
		IADJUST;                                          \\
		IADJUST;                                          \\
		break;                                            \\
          caseop(I_N4E):                                          \\
		IDIRECT;                                          \\
		IDIRECT;                                          \\
 		IADJUST;                                          \\
		IADJUST;                                          \\
		IADJUST;                                          \\
		IADJUST;                                          \\
		break;                                            \\
          caseop(I_HTAB):		                                  \\
 		IHASH;                                            \\
   		break;                                            \\
          default:                                                \\
		printf("Something wrong during adjusting code entries\n");   \\
	}

	/*
 	  switch (opcode_tab[*tp2].type) {

			caseop(I_STATIONARY):
			caseop(I_WORKER):
			caseop(I_MESSENGER):
			caseop(I_E):		// label
				*tp1++ = *tp2++;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

	  		caseop(I_O):
 	  		 	*tp1++ = *tp2++;
	  			break;

			caseop(I_N):		// number
 			caseop(I_C):		// constant (int/chars)
			caseop(I_F):		// functor
			caseop(I_B):		// builtin
  			caseop(I_I):		// integer
 			caseop(I_P):		// procedure
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				break;

			caseop(I_R):		// floating value
			caseop(I_2N):
			caseop(I_NC):
			caseop(I_NF):		// integer functor
			caseop(I_NI):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
  				break;

			caseop(I_3N):
 			caseop(I_NR):
			caseop(I_2NF):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
   				break;

			caseop(I_4N):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
   				break;

 			caseop(I_5N):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
  				break;

			caseop(I_6N):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
  				break;

			caseop(I_7N):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
  				break;

			caseop(I_8N):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
   				break;

			caseop(I_NE):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_2NE):
			caseop(I_NFE):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_2NFE):
 			caseop(I_3NE):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_3NFE):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
 				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_4E):
				*tp1++ = *tp2++;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_N4E):
				*tp1++ = *tp2++;
				*tp1++ = *tp2++;
 				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				(int*) *tp1++ = (int*) *tp2++ + distance;
				break;

			caseop(I_HTAB):		// hashtable
				*tp1++ = *tp2++;
				v = *tp1++ = *tp2++; // get table size (one more for var)
 				(int*) *tp1++ = (int*) *tp2++ + distance; // copy var entry
 				while (v--){
					*tp1++ = *tp2++;
					(int*) *tp1++ = (int*) *tp2++ + distance;
 				}
   				break;
			default:
				printf("Do_expansion: something wrong during adjusting code entries\n");
	 	 }
		*/
