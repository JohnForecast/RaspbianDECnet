/*  Note: You are free to use whatever license you want.
    Eventually you will be able to edit it within Glade. */

/*  phone
 *  Copyright (C) <YEAR> <AUTHORS>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <gtk/gtk.h>

void
on_Quit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Dial_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Answer_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Reject_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Hold_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Directory_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Commands_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_About_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Facsimile_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_Hangup_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_LocalText_key_press_event           (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);
void
on_DialOK_activate                      (GtkMenuItem     *menuitem,
					 gpointer         user_data);

void
on_DirOK_activate                      (GtkMenuItem     *menuitem,
					 gpointer         user_data);

void
on_DialCancel_activate                   (GtkMenuItem     *menuitem,
					  gpointer         user_data);


void
on_FacsimileOK_activate                  (GtkMenuItem     *menuitem,
					  gpointer         user_data);

void
on_DisplayOK_activate                       (GtkMenuItem     *menuitem,
					     gpointer         user_data);
