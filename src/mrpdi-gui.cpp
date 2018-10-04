

#include <fstream>
#include <iostream>
#include <map>
#include <gtkmm.h>
#include <glibmm.h>
#include <dlfcn.h>
#include <libintl.h>
 #include <locale.h>

#include <mrpdi/Core.h>
#include <mrpdi/Input.h>

#include <libconfig.h++>
#include <libappindicator/app-indicator.h>


#define T(String) gettext(String)



using namespace std;
using namespace net::lliurex::mrpdi;

/**
 * Application class
 */
class Application
{
	public:
	
	Gtk::Window * window;
	Gtk::Window * dlgClose;
	
	Gtk::TreeView * treeview;
	Core * core;
	input::InputHandler * handler;
	Glib::RefPtr <Gdk::Pixbuf> pix_tablet,pix_whiteboard;
	Glib::RefPtr<Gtk::ListStore> store;
	Glib::RefPtr<Gtk::StatusIcon> statusIcon;
	
	AppIndicator *indicator;
	
	
	map<unsigned int,input::DeviceSettingsEntry> settings_map;
	bool corrupted_settings;
	
	
	/**
	 * Constructor
	 */ 
	Application()
	{
		cout<<"* Starting MrPDI gui"<<endl;
		
		load_setup();
		
		Glib::RefPtr<Gtk::Builder> glade;
		glade=Gtk::Builder::create_from_file("/usr/share/mrpdi/gui/interface.glade");
		glade->get_widget("winManager",window);	
		
		Gtk::Button * button;
		glade->get_widget("btnRefresh",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btnrefresh_clicked));
		
		glade->get_widget("btnRun",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btnrun_clicked));
		
		glade->get_widget("btnStop",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btnstop_clicked));
		
		glade->get_widget("btnDebug",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btndebug_clicked));
				
		glade->get_widget("btnCalibrate",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btncalibrate_clicked));
		
		glade->get_widget("btnClose",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_btnclose_clicked));
		
		glade->get_widget("dlgClose",dlgClose);
		glade->get_widget("dlgbtnCancel",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_dlg_cancel));
		
		glade->get_widget("dlgbtnAccept",button);
		button->signal_clicked().connect(sigc::mem_fun(*this,&Application::on_dlg_accept));
				
		window->signal_window_state_event().connect(sigc::mem_fun(*this,&Application::on_window_state_changed));
		window->signal_delete_event().connect(sigc::mem_fun(*this,&Application::on_window_destroy));
		
		glade->get_widget("treeDevices",treeview);
		
		
		pix_tablet = Gdk::Pixbuf::create_from_file("/usr/share/mrpdi/gui/tablet.svg");
		pix_whiteboard = Gdk::Pixbuf::create_from_file("/usr/share/mrpdi/gui/whiteboard.svg");

		Gtk::TreeModelColumn<Glib::RefPtr<Gdk::Pixbuf> >pixbuf_col;		
		Gtk::TreeModelColumn<unsigned int> id_col;
		Gtk::TreeModelColumn<unsigned int> address_col;
		Gtk::TreeModelColumn<string> name_col; 
		Gtk::TreeModelColumn<string> status_col; 
		 
		Gtk::TreeModel::ColumnRecord cr;
		cr.add(pixbuf_col);
		cr.add(address_col);
		cr.add(id_col);
		cr.add(name_col);
		cr.add(status_col);
		
		
		store=Gtk::ListStore::create(cr);
		
		treeview->set_model(store);
		treeview->append_column(T("Type"),pixbuf_col);
		//treeview->append_column_numeric(T("Id"),id_col,"%08x");
		//treeview->append_column_numeric(T("Address"),address_col,"%08x");
		treeview->append_column(T("Name"),name_col);
		treeview->append_column(T("Status"),status_col);
		
        
		window->show_all();
             
        
	
		indicator = app_indicator_new ("mrpdi","input-tablet", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
		app_indicator_set_status (indicator, APP_INDICATOR_STATUS_ACTIVE);
		app_indicator_set_attention_icon (indicator, "input-tablet");
		
		
		Gtk::Menu * menu = new Gtk::Menu();
		Gtk::MenuItem * itm_show=new Gtk::MenuItem(T("Show"));
		
		
		menu->append(*itm_show);
		
		itm_show->signal_activate().connect(sigc::mem_fun(*this,&Application::on_indicator_show_click));
		
		menu->show_all();
		
		app_indicator_set_menu(indicator,menu->gobj());
		
		core = new Core();
		handler = new input::InputHandler();
		core->init();
		
		core->set_input_handler(handler);
		handler->set_settings(settings_map);
		
		refresh();
					
			   
	       Glib::signal_timeout().connect(sigc::mem_fun(*this,&Application::on_timer), 1000);
		
	}
	
	
	/**
	 * Destructor
	 * things should be closed here
	 */ 
	~Application()
	{
		cout<<"* Shutting down MrPDI gui"<<endl;
		core->shutdown();
		
		settings_map = handler->get_settings();
			if(!corrupted_settings)
				save_setup();
		
		delete handler;
        delete core;
		
	}
	
	void on_indicator_show_click()
	{
		window->deiconify();
		window->show_all();
	}
	
	/**
	 * loads calibration setup
	 * */
	void load_setup()
	{
		struct passwd * pwd;
		
		cout<<"* Loading settings"<<endl;
		
		string path("/etc/mrpdi/settings.conf");
		
		libconfig::Config cfg;
			
		
		ifstream myfile(path.c_str(),std::ifstream::in);
		
		if(myfile.good())
		{
			myfile.close();
			
			try
			{
				cfg.readFile(path.c_str());
				
				libconfig::Setting & version = cfg.lookup("version");
				std::string version_str=version;
				
				if(version_str!="2.0")
				{
					cout<<"Warning: Unknown format version: "<<version_str<<endl;
				}
			
				libconfig::Setting & setting = cfg.lookup("mrpdi.devices");
						
				for(int n=0;n<setting.getLength();n++)
				{
					unsigned int id;
					
					string name;
					setting[n].lookupValue("name",name);
					
					id=setting[n]["id"];
					
					settings_map[id].id=id; //and fuck that redundant shit
					settings_map[id].name=name;
								
					for(int m=0;m<8;m++)
					{
						settings_map[id].calibration[m]=setting[n]["calibration"][m];
					}
					
					
					cout<<"name:"<<name<<endl;
					cout<<"params: "<<setting[n]["params"].getLength()<<endl;
					for(int m=0;m<setting[n]["params"].getLength();m++)
					{
						string name;
						unsigned int value;
						setting[n]["params"][m].lookupValue("name",name);
						setting[n]["params"][m].lookupValue("value",value);
						cout<<"* "<<name<<":"<<value<<endl;
						settings_map[id].params[name]=value;
					}
					
				}//for
				
				
				
			}//try
			catch(libconfig::ParseException &e)
			{
				cerr<<"* Error parsing config file"<<endl;
				settings_map.clear();
				corrupted_settings=true;
			}
		}//if
		
			
		
	}

	/**
	 * saves 
	 **/ 
	void save_setup()
	{
		cout<<"* Saving settings"<<endl;
		
		string path("/etc/mrpdi/settings.conf");
		
		libconfig::Config cfg;
			
		libconfig::Setting & root = cfg.getRoot();
		libconfig::Setting & version = root.add("version",libconfig::Setting::TypeString);
		version="2.0";
		
		libconfig::Setting & mrpdi = root.add("mrpdi",libconfig::Setting::TypeGroup);
		libconfig::Setting & devices = mrpdi.add("devices",libconfig::Setting::TypeList);
		
		std::map<unsigned int,input::DeviceSettingsEntry>::iterator it;
				
		for(it=settings_map.begin();it!=settings_map.end();it++)
		{
			libconfig::Setting & device = devices.add(libconfig::Setting::TypeGroup);
			//id
			libconfig::Setting & id = device.add("id",libconfig::Setting::TypeInt);
			id.setFormat(libconfig::Setting::FormatHex);
			id=(int)it->first;
			//name
			libconfig::Setting & name = device.add("name",libconfig::Setting::TypeString);
			name=it->second.name;
			//calibration
			libconfig::Setting & calibration = device.add("calibration",libconfig::Setting::TypeArray);
			for(int n=0;n<8;n++)
			{
				libconfig::Setting & f = calibration.add(libconfig::Setting::TypeFloat);
				f=it->second.calibration[n];
			}
			
			//params	
			libconfig::Setting & params = device.add("params",libconfig::Setting::TypeList);
			std::map<string,unsigned int>::iterator pit;
			
			for(pit=it->second.params.begin();pit!=it->second.params.end();pit++)
			{
				cout<<"* name:"<<pit->first<<" value:"<<pit->second<<endl;
				libconfig::Setting & param = params.add(libconfig::Setting::TypeGroup);
				libconfig::Setting & pname = param.add("name",libconfig::Setting::TypeString);
				pname=pit->first;
				libconfig::Setting & pvalue = param.add("value",libconfig::Setting::TypeInt);
				pvalue=(int)pit->second;
			}
			
		}
			
		
		cfg.writeFile(path.c_str());
		
		
	}
	
	
	/**
	 * Refresh device table
	 */ 
	void refresh()
	{
		vector<Gtk::TreeModel::Path> selection; 
		vector<connected_device_info> list;
        
        core->update_devices(&list);
        
        selection=treeview->get_selection()->get_selected_rows();
        store->clear();
        
        for(int n=0;n<list.size();n++)
        {
			
			Gtk::TreeModel::Row row= *(store->append());
			if(list[n].type==0)row.set_value<Glib::RefPtr<Gdk::Pixbuf> >(0,pix_tablet);
				else row.set_value<Glib::RefPtr<Gdk::Pixbuf> >(0,pix_whiteboard);
				
			row.set_value<unsigned int>(1,list[n].address);
			row.set_value<unsigned int>(2,list[n].id);
			row.set_value<string>(3,list[n].name);
			
			if(list[n].status==1)row.set_value<string>(4,T("Running"));
				else row.set_value<string>(4,T("Stop"));
			
			vector<string> parameters;
			core->get_parameter_list(list[n].id,&parameters);
		
		}
		
		if(selection.size()>0)
			treeview->get_selection()->select(selection[0]);
	}
	
	/**
	 * Window state hook
	 */  
	bool on_window_state_changed(GdkEventWindowState* event)
	{
		cout<<"* State:"<<event->new_window_state<<endl;
		
		if( (event->new_window_state & GDK_WINDOW_STATE_ICONIFIED) == GDK_WINDOW_STATE_ICONIFIED)
		{
			cout<<"* Minimize!"<<endl;
			window->hide();
		}
		return false;
	}
	
	
	
	/**
	 * Timer handler
	 */ 
	int on_timer()
	{
		//cout<<"tic tac!!!"<<endl;
		refresh();
	}
	
	/**
	* Dialog cancel
	*/
	void on_dlg_cancel()
	{
		dlgClose->hide();
	}
	
	/**
	* Dialog accept
	*/
	void on_dlg_accept()
	{
		core->shutdown();
		Gtk::Main::quit();
	}
	
	
	/**
	* Close button
	*/
	void on_btnclose_clicked()
	{
		dlgClose->show_all();	
	}
	
	/**
	 * Calibrate
	 */  
	 void on_btncalibrate_clicked()
	 {
		Glib::RefPtr<Gtk::TreeSelection> selection;
		Gtk::TreeModel::iterator iter;
		unsigned int id,address,value;
		
		cout<<"* Calibrate"<<endl;
		selection = treeview->get_selection();
		iter=selection->get_selected();
		if(iter)
		{
			Gtk::TreeModel::Row row = *iter;
			row.get_value(2,id);
			row.get_value(1,address);
			cout<<"* Debugging "<<hex<<address<<endl;
			
			handler->calibrate(address);
						
			
		}
	 }
	
	/**
	 * Debug mode
	 */ 
	void on_btndebug_clicked()
	{
		Glib::RefPtr<Gtk::TreeSelection> selection;
		Gtk::TreeModel::iterator iter;
		unsigned int id,address,value;
		
		cout<<"* Debug"<<endl;
		selection = treeview->get_selection();
		iter=selection->get_selected();
		if(iter)
		{
			Gtk::TreeModel::Row row = *iter;
			row.get_value(2,id);
			row.get_value(1,address);
			cout<<"* Debugging "<<hex<<address<<endl;
			
			core->get_parameter(id,"common.debug",&value);
			core->set_parameter(id,"common.debug",(~value) & 1);
			
		}
		
	}
	
	/**
	 * Refresh button click signal
	 */ 
	void on_btnrefresh_clicked()
	{
		cout<<"* Refresh"<<endl;
		refresh();
	}
	/**
	 * Run button clicked
	 */ 
	void on_btnrun_clicked()
	{
		Glib::RefPtr<Gtk::TreeSelection> selection;
		Gtk::TreeModel::iterator iter;
		unsigned int id,address;
		
		cout<<"* Run"<<endl;
		
		selection = treeview->get_selection();
		iter=selection->get_selected();
		if(iter)
		{
			Gtk::TreeModel::Row row = *iter;
			row.get_value(2,id);
			row.get_value(1,address);
			cout<<"* Turning on "<<hex<<address<<endl;
			core->start(id,address);
			
		}
		
		
		
	}
	/**
	 * Stop button clicked
	 */ 
	void on_btnstop_clicked()
	{
		
		Glib::RefPtr<Gtk::TreeSelection> selection;
		Gtk::TreeModel::iterator iter;
		unsigned int id,address;
		
		cout<<"* Stop"<<endl;
		
		selection = treeview->get_selection();
		iter=selection->get_selected();
		if(iter)
		{
			Gtk::TreeModel::Row row = *iter;
			row.get_value(2,id);
			row.get_value(1,address);
			cout<<"* Turning off "<<hex<<address<<endl;
			core->stop(id,address);
			
		}
	}
	
	/**
	 * Destroy window event
	 */ 
	bool on_window_destroy(GdkEventAny* event)
	{
		
		dlgClose->show_all();
		return true;
	}
	
	
};


/**
 * Main
 */ 
int main(int argc,char * argv[])
{
	textdomain("mrpdi-gui");
	Gtk::Main kit(argc, argv);
	Application app; 
	Gtk::Main::run();
	
	return 0;
}
