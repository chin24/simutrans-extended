/*
 * Copyright (c) 1997 - 2001 Hansj�rg Malthaner
 *
 * This file is part of the Simutrans project under the artistic licence.
 * (see licence.txt)
 */

#include <stdio.h>

#include "convoi_info_t.h"
#include "replace_frame.h"

#include "../simconvoi.h"
#include "../simdepot.h"
#include "../vehicle/simvehikel.h"
#include "../simcolor.h"
#include "../simgraph.h"
#include "../simworld.h"
#include "../simwin.h"

#include "../dataobj/fahrplan.h"
#include "../dataobj/translator.h"
#include "../dataobj/umgebung.h"
#include "fahrplan_gui.h"
// @author hsiegeln
#include "../simlinemgmt.h"
#include "../simline.h"
#include "../boden/grund.h"
#include "messagebox.h"

#include "../utils/simstring.h"

#include "components/list_button.h"

#include "convoi_detail_t.h"

const char cost_type[MAX_CONVOI_COST][64] =
{
	"Free Capacity",
	"Transported",
	"Average speed",
	"Comfort",
	"Revenue",
	"Operation",
	"Profit"
};

//bool convoi_info_t::route_search_in_progress = false;

/**
 * This variable defines by which column the table is sorted
 * Values:			0 = destination
 *                  1 = via
 *                  2 = via_amount
 *                  3 = amount
 *					4 = origin
 *					5 = origin_amount
 * @author prissi - amended by jamespetts (origins)
 */
const char *convoi_info_t::sort_text[SORT_MODES] = 
{
	"Zielort",
	"via",
	"via Menge",
	"Menge",
	"origin (detail)",
	"origin (amount)"
};

const int cost_type_color[MAX_CONVOI_COST] =
{
	COL_FREE_CAPACITY, COL_TRANSPORTED, COL_AVERAGE_SPEED, COL_COMFORT, COL_REVENUE, COL_OPERATION, COL_PROFIT
};


convoi_info_t::convoi_info_t(convoihandle_t cnv)
:	gui_frame_t(cnv->get_name(), cnv->get_besitzer()),
	scrolly(&text),
	text(" \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n"
			 " \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n"
			 " \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n"
			 " \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n"
			 " \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n \n"),
	view(cnv->get_vehikel(0)),
	sort_label(translator::translate("loaded passenger/freight")),
	freight_info(8192)
{
	this->cnv = cnv;
	this->mean_convoi_speed = speed_to_kmh(cnv->get_akt_speed()*4);
	this->max_convoi_speed = speed_to_kmh(cnv->get_min_top_speed()*4);

	input.set_pos(koord(11,4));
	input.set_text( cnv->access_internal_name(), 116);
	add_komponente(&input);

	add_komponente(&sort_label);

	toggler.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	toggler.set_text("Chart");
	toggler.set_typ(button_t::roundbox_state);
	toggler.add_listener(this);
	toggler.set_tooltip("Show/hide statistics");
	add_komponente(&toggler);
	toggler.pressed = false;

	sort_button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	sort_button.set_text(sort_text[umgebung_t::default_sortmode]);
	sort_button.set_typ(button_t::roundbox);
	sort_button.add_listener(this);
	sort_button.set_tooltip("Sort by");
	add_komponente(&sort_button);

	details_button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	details_button.set_text("Details");
	details_button.set_typ(button_t::roundbox);
	details_button.add_listener(this);
	details_button.set_tooltip("Vehicle details");
	add_komponente(&details_button);

	scrolly.set_pos(koord(0, 122));
	add_komponente(&scrolly);

	filled_bar.add_color_value(&cnv->get_loading_limit(), COL_YELLOW);
	filled_bar.add_color_value(&cnv->get_loading_level(), COL_GREEN);
	add_komponente(&filled_bar);

	speed_bar.set_base(max_convoi_speed);
	speed_bar.set_vertical(false);
	speed_bar.add_color_value(&mean_convoi_speed, COL_GREEN);
	add_komponente(&speed_bar);

	// we update this ourself!
	route_bar.add_color_value(&cnv_route_index, COL_GREEN);
	add_komponente(&route_bar);

	// goto line button
	line_button.init( button_t::posbutton, NULL, koord(10, 64) );
	line_button.set_targetpos( koord(0,0) );
	line_button.add_listener( this );
	line_bound = false;

	set_fenstergroesse(koord(TOTAL_WIDTH, 278));

	// chart
	chart.set_pos(koord(44,76+BUTTON_HEIGHT+18));
 	chart.set_groesse(koord(TOTAL_WIDTH-44-4, 100));
 	chart.set_dimension(12, 10000);
 	chart.set_visible(false);
	chart.set_groesse(koord(TOTAL_WIDTH-44-4, 100));
	chart.set_dimension(12, 10000);
	chart.set_visible(false);
	chart.set_background(MN_GREY1);
	for (int cost = 0; cost<MAX_CONVOI_COST; cost++) {
		chart.add_curve(cost_type_color[cost], cnv->get_finance_history(), MAX_CONVOI_COST, cost, MAX_MONTHS, cost<MAX_CONVOI_NON_MONEY_TYPES ? 0 : 1, false, true);
		filterButtons[cost].init(button_t::box_state, cost_type[cost], koord(BUTTON1_X+(BUTTON_WIDTH+BUTTON_SPACER)*(cost%4), 230+(BUTTON_HEIGHT+2)*(cost/4)), koord(BUTTON_WIDTH, BUTTON_HEIGHT));
		filterButtons[cost].add_listener(this);
		filterButtons[cost].background = cost_type_color[cost];
		filterButtons[cost].set_visible(false);
		filterButtons[cost].pressed = false;
		add_komponente(filterButtons + cost);
	}
	add_komponente(&chart);
	add_komponente(&view);

	// this convoi belongs not to an AI
	button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	button.set_text("Fahrplan");
	button.set_typ(button_t::roundbox);
	button.set_tooltip("Alters a schedule.");
	add_komponente(&button);
	button.set_pos(koord(BUTTON1_X,76));
	button.add_listener(this);

	go_home_button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	go_home_button.set_pos(koord(BUTTON2_X,76));
	go_home_button.set_text("go home");
	go_home_button.set_typ(button_t::roundbox);
	go_home_button.set_tooltip("Sends the convoi to the last depot it departed from!");
	add_komponente(&go_home_button);
	go_home_button.add_listener(this);

	no_load_button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	no_load_button.set_pos(koord(BUTTON3_X,76));
	no_load_button.set_text("no load");
	no_load_button.set_typ(button_t::roundbox);
	no_load_button.set_tooltip("No goods are loaded onto this convoi.");
	add_komponente(&no_load_button);
	no_load_button.add_listener(this);

	replace_button.set_groesse(koord(BUTTON_WIDTH, BUTTON_HEIGHT));
	replace_button.set_pos(koord(BUTTON3_X,76+BUTTON_HEIGHT+1));
	replace_button.set_text("Replace");
	replace_button.set_typ(button_t::box);
	replace_button.set_tooltip("Automatically replace this convoy.");
	add_komponente(&replace_button);
	replace_button.add_listener(this);

	follow_button.set_groesse(koord(66, BUTTON_HEIGHT));
	follow_button.set_text("follow me");
	follow_button.set_typ(button_t::roundbox_state);
	follow_button.set_tooltip("Follow the convoi on the map.");
	add_komponente(&follow_button);
	follow_button.add_listener(this);

	cnv->set_sortby( umgebung_t::default_sortmode );

	set_min_windowsize(koord(TOTAL_WIDTH, 194));
	set_resizemode(diagonal_resize);
	resize(koord(0,0));
}




/**
 * komponente neu zeichnen. Die �bergebenen Werte beziehen sich auf
 * das Fenster, d.h. es sind die Bildschirkoordinaten des Fensters
 * in dem die Komponente dargestellt wird.
 *
 * Component draw again. The handed over values refer to the window,
 * i.e. there is the Bildschirkoordinaten of the window in that the
 * component is represented. (Babelfish)
 *
 * @author Hj. Malthaner
 */
void
convoi_info_t::zeichnen(koord pos, koord gr)
{
	if(!cnv.is_bound() || cnv->in_depot() || cnv->get_vehikel_anzahl() == 0) 
	{
		destroy_win(dynamic_cast <gui_fenster_t *> (this));
	}
	else {
		if(cnv->get_besitzer()==cnv->get_welt()->get_active_player()) {
			if(  line_bound  &&  !cnv->get_line().is_bound()  ) {
				remove_komponente( &line_button );
				line_bound = false;
			}
			else if(  !line_bound  &&  cnv->get_line().is_bound()  ) {
				add_komponente( &line_button );
				line_bound = true;
			}
			button.enable();
			//go_home_button.pressed = route_search_in_progress;
			details_button.pressed = win_get_magic( magic_convoi_detail+cnv.get_id() );
			go_home_button.enable(); // Will be disabled, if convoy goes to a depot.
			if(  cnv->get_schedule()->get_count() > 0  ) {
				const grund_t* g = cnv->get_welt()->lookup(cnv->get_schedule()->get_current_eintrag().pos);
				if (g != NULL && g->get_depot()) {
					go_home_button.disable();
				}
				else {
					goto enable_home;
				}
			}
			else 
			{
enable_home:
				go_home_button.enable();
			}
			no_load_button.pressed = cnv->get_no_load();
			no_load_button.enable();
			replace_button.background= cnv->get_replace()?COL_LIGHT_RED:MN_GREY3;
			replace_button.set_text(cnv->get_replace()?"Replacing":"Replace");
			replace_button.enable();
		}
		else {
			if(  line_bound  ) {
				// do not jump to other player line window
				remove_komponente( &line_button );
				line_bound = false;
			}
			button.disable();
			go_home_button.disable();
			no_load_button.disable();
			replace_button.disable();
		}
		follow_button.pressed = (cnv->get_welt()->get_follow_convoi()==cnv);

		// buffer update now only when needed by convoi itself => dedicated buffer for this
		cnv->get_freight_info(freight_info);
		text.set_text(freight_info);

		route_bar.set_base(cnv->get_route()->get_max_n());
		cnv_route_index = cnv->get_vehikel(0)->get_route_index()-1;

		// all gui stuff set => display it
		gui_frame_t::zeichnen(pos, gr);

		PUSH_CLIP(pos.x+1,pos.y+16,gr.x-2,gr.y-16);

		// convoi information
		char tmp[256];
		static cbuffer_t info_buf(256);

		// use median speed to avoid flickering
		mean_convoi_speed += speed_to_kmh(cnv->get_akt_speed()*4);
		mean_convoi_speed /= 2;
		sprintf(tmp,translator::translate("%i km/h (max. %ikm/h)"), (mean_convoi_speed+3)/4, speed_to_kmh(cnv->get_min_top_speed()) );
		display_proportional( pos.x+11, pos.y+16+20, tmp, ALIGN_LEFT, COL_BLACK, true );

		// next important: income stuff
		int len = display_proportional(pos.x + 11, pos.y + 16 + 20 + 1 * LINESPACE, translator::translate("Gewinn"), ALIGN_LEFT, COL_BLACK, true ) + 5;
		money_to_string( tmp, cnv->get_jahresgewinn()/100.0 );
		len += display_proportional( pos.x+11+len, pos.y+16+20+1*LINESPACE, tmp, ALIGN_LEFT, cnv->get_jahresgewinn()>0?MONEY_PLUS:MONEY_MINUS, true )+5;
		sprintf(tmp," (%1.2f$/km)", cnv->get_running_cost()/100.0 );
		display_proportional( pos.x+11+len, pos.y+16+20+1*LINESPACE, tmp, ALIGN_LEFT, COL_BLACK, true );

		// the weight entry
		info_buf.clear();
		info_buf.append( translator::translate("Gewicht") );
		info_buf.append( ": " );
		info_buf.append( cnv->get_sum_gesamtgewicht() );
		info_buf.append( " (" );
		info_buf.append( cnv->get_sum_gesamtgewicht()-cnv->get_sum_gewicht() );
		info_buf.append( ") t" );
		display_proportional( pos.x+11, pos.y+16+20+2*LINESPACE, info_buf, ALIGN_LEFT, COL_BLACK, true );

		// next stop
		const schedule_t * fpl = cnv->get_schedule();
		info_buf.clear();
		info_buf.append(translator::translate("Fahrtziel"));
		fahrplan_gui_t::gimme_short_stop_name(info_buf, cnv->get_welt(), cnv->get_besitzer(), fpl, fpl->get_aktuell(), 34);
		len = display_proportional_clip( pos.x+11, pos.y+16+20+3*LINESPACE, info_buf, ALIGN_LEFT, COL_BLACK, true );

		// convoi load indicator
		const int offset = max( len, 167)+3;
		route_bar.set_pos(koord(offset,22+3*LINESPACE));
		route_bar.set_groesse(koord(view.get_pos().x-offset-5, 4));

		/*
		 * only show assigned line, if there is one!
		 * @author hsiegeln
		 */
		if(  cnv->get_line().is_bound()  ) {
			sint16 add_off = line_bound*12;
			sint16 w = display_proportional( pos.x+11+add_off, pos.y+16+20+4*LINESPACE, translator::translate("Serves Line:"), ALIGN_LEFT, COL_BLACK, true );
			display_proportional_clip( pos.x+11+w+5+add_off, pos.y+16+20+4*LINESPACE, cnv->get_line()->get_name(), ALIGN_LEFT, cnv->get_line()->get_state_color(), true );
		}
		POP_CLIP();
	}
}


/**
 * This method is called if an action is triggered
 * @author Hj. Malthaner
 */
bool convoi_info_t::action_triggered( gui_action_creator_t *komp,value_t /* */)
{
	// follow convoi on map?
	if(komp == &follow_button) {
		if(cnv->get_welt()->get_follow_convoi()==cnv) {
			// stop following
			cnv->get_welt()->set_follow_convoi( convoihandle_t() );
		}
		else {
			cnv->get_welt()->set_follow_convoi(cnv);
		}
		return true;
	}

	// datails?
	if(komp == &details_button) {
		create_win(20, 20, new convoi_detail_t(cnv), w_info, magic_convoi_detail+cnv.get_id() );
		return true;
	}

	if(  komp == &line_button  ) {
		cnv->get_besitzer()->simlinemgmt.show_lineinfo( cnv->get_besitzer(), cnv->get_line() );
		cnv->get_welt()->set_dirty();
	}

	// sort by what
	if(komp == &sort_button) {
		// sort by what
		umgebung_t::default_sortmode = (sort_mode_t)((int)(cnv->get_sortby()+1)%(int)SORT_MODES);
		sort_button.set_text(sort_text[umgebung_t::default_sortmode]);
		cnv->set_sortby( umgebung_t::default_sortmode );
	}

	// some actions only allowed, when I am the player
	if(cnv->get_besitzer()==cnv->get_welt()->get_active_player()) {

		if(komp == &button) {
			cnv->open_schedule_window();
			return true;
		}

		//if(komp == &no_load_button    &&    !route_search_in_progress) 
		if(komp == &no_load_button)
		{
			cnv->set_no_load(!cnv->get_no_load());
			if(!cnv->get_no_load()) {
				cnv->set_withdraw(false);
			}
			return true;
		}

		if(komp == &replace_button) {
			if (cnv->get_replace()) {
				cnv->set_replace(false);
				return true;
			}
			create_win(20, 20, new replace_frame_t(cnv, get_name()), w_info, (long)this);
			return true;
		}
		//if(komp == &go_home_button    &&    !route_search_in_progress) 
		if(komp == &go_home_button) 
		{
			cnv->go_to_depot(true);
			return true;
		} // end go home button
	}

	if (komp == &toggler) {
		toggler.pressed = !toggler.pressed;
		const koord offset = toggler.pressed ? koord(0, 170) : koord(0, -170);
		set_min_windowsize( koord(TOTAL_WIDTH, toggler.pressed ? 364: 194));
		scrolly.set_pos( scrolly.get_pos()+koord(0,offset.y) );
		// toggle visibility of components
		chart.set_visible(toggler.pressed);
		set_fenstergroesse(get_fenstergroesse() + offset); // "Window size"
		resize(koord(0,0));
		for (int i=0;i<MAX_CONVOI_COST;i++) {
			filterButtons[i].set_visible(toggler.pressed);
		}
		return true;
	}

	for ( int i = 0; i<MAX_CONVOI_COST; i++) {
		if (komp == &filterButtons[i]) {
			filterButtons[i].pressed = !filterButtons[i].pressed;
			if(filterButtons[i].pressed) {
				chart.show_curve(i);
			} else {
				chart.hide_curve(i);
			}

			return true;
		}
	}

	return false;
}


/**
 * Resize the contents of the window
 * @author Markus Weber
 */
void convoi_info_t::resize(const koord delta)
{
	gui_frame_t::resize(delta);

	input.set_groesse(koord(get_fenstergroesse().x-22, 13));

	view.set_pos(koord(get_fenstergroesse().x - 64 - 12 , 21));
	follow_button.set_pos(koord(view.get_pos().x-1,77));

	scrolly.set_groesse(get_client_windowsize()-scrolly.get_pos());

	const int yoff = scrolly.get_pos().y-BUTTON_HEIGHT-2;
	sort_button.set_pos(koord(BUTTON1_X,yoff));
	toggler.set_pos(koord(BUTTON3_X,yoff));
	details_button.set_pos(koord(BUTTON4_X,yoff));
	sort_label.set_pos(koord(BUTTON1_X,yoff-LINESPACE));

	// convoi speed indicator
	speed_bar.set_pos(koord(170,22+0*LINESPACE));
	speed_bar.set_groesse(koord(view.get_pos().x - 175, 4));

	// convoi load indicator
	filled_bar.set_pos(koord(170,22+2*LINESPACE));
	filled_bar.set_groesse(koord(view.get_pos().x - 175, 4));
}
