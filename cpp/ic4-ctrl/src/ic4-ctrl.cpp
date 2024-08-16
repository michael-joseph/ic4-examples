﻿
#include <ic4/ic4.h>

#ifdef _WIN32

#include <ic4-gui/ic4gui.h>

#endif


#include <CLI/CLI.hpp>
#include <fmt/core.h>


#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <stdexcept>

#include "ic4_enum_to_string.h"
#include "ic4-ctrl-helper.h"

static void    print_property( int offset, const ic4::Property& property );

template<class ... Targs>
void print( fmt::format_string<Targs...> fmt, Targs&& ... args )
{
    fmt::print( fmt, std::forward<Targs>( args )... );
}

template<class ... Targs>
void print( int offset, fmt::format_string<Targs...> fmt, Targs&& ... args )
{
    for( int i = 0; i < offset; ++i ) {
        fmt::print( "    " );
    }
    fmt::print( fmt, std::forward<Targs>( args )... );
}

static auto find_device( std::string id ) -> std::unique_ptr<ic4::DeviceInfo>
{
    ic4::DeviceEnum devEnum;
    auto list = devEnum.enumDevices();
    if( list.size() == 0 ) {
        throw std::runtime_error( "No devices are available" );
    }

    for( auto&& dev : list )
    {
        if( dev.serial() == id ) {
            return std::make_unique<ic4::DeviceInfo>( dev );
        }
    }
    for( auto&& dev : list )
    {
        if( dev.uniqueName() == id ) {
            return std::make_unique<ic4::DeviceInfo>( dev );
        }
    }
    for( auto&& dev : list )
    {
        if( dev.modelName() == id ) {
            return std::make_unique<ic4::DeviceInfo>( dev );
        }
    }
    int64_t index = 0;
    if( helper::from_chars_helper( id, index ) )
    {
        if( index < 0 || index >= static_cast<int64_t>(list.size()) )
            return {};

        return std::make_unique<ic4::DeviceInfo>( list.at( index ) );
    }
    return {};
}

static auto find_interface( std::string id ) -> std::unique_ptr<ic4::Interface>
{
    ic4::DeviceEnum devEnum;
    auto list = devEnum.enumInterfaces();
    if( list.size() == 0 ) {
        throw std::runtime_error( "No devices are available" );
    }

    for( auto&& dev : list )
    {
        if( dev.interfaceDisplayName() == id ) {
            return std::make_unique<ic4::Interface>( dev );
        }
    }
    for( auto&& dev : list )
    {
        if( dev.transportLayerName() == id ) {
            return std::make_unique<ic4::Interface>( dev );
        }
    }
    int64_t index = 0;
    if( helper::from_chars_helper( id, index ) )
    {
        if( index < 0 || index >= static_cast<int64_t>(list.size()) )
            return {};

        return std::make_unique<ic4::Interface>( list.at( index ) );
    }
    return {};
}

static auto list_all_by_connection() -> void
{
	ic4::DeviceEnum devEnum;
	auto list = devEnum.enumInterfaces();

	print("Device tree:\n");
	if (list.empty()) {
		print(1, "No Interfaces found\n");
	}
	else
	{
		std::set<std::string> transport_layer_list;

		for (auto&& e : list) {
			transport_layer_list.insert(e.transportLayerName());
		}
		for (auto&& transportLayerName : transport_layer_list)
		{
			print(1, "TransportLayer: {}\n", transportLayerName);
			for (size_t i = 0; i < list.size(); ++i)
			{
				auto& itf = list.at(i);
				if (transportLayerName == itf.transportLayerName())
				{
					print(2, "{}\n", itf.interfaceDisplayName());

					auto dev_list = itf.enumDevices();
					if (dev_list.empty()) {
						print(3, "No devices\n");
					}
					else
					{
						for (auto&& device : dev_list) {
							print(3, "{:24} {:8}\n", device.modelName(), device.serial());
						}
					}
					print("\n");
				}
			}
			print("\n");
		}
	}
}

static auto list_devices() -> void
{
	ic4::DeviceEnum devEnum;
	auto list = devEnum.enumDevices();

	print("Device list:\n");
	if (list.empty()) {
		print(1, "No devices found\n");
		return;
	}

	print(1, "Index   {:24} {:8} {}\n", "ModelName", "Serial", "InterfaceName");
	int index = 0;
	for (auto&& e : list) {
		print(1, "{:^5}   {:24} {:8} {}\n", index, e.modelName(), e.serial(), e.getInterface().transportLayerName());
		index += 1;
	}
}

static auto list_interfaces() -> void
{
    ic4::DeviceEnum devEnum;
    auto list = devEnum.enumInterfaces();

    print( "Interface list:\n" );
	if (list.empty()) {
		print(1, "No Interfaces found\n");
	}
	else
	{
		std::set<std::string> transport_layer_list;

		for (auto&& e : list) {
			transport_layer_list.insert(e.transportLayerName());
		}
		for (auto&& transportLayerName : transport_layer_list)
		{
			print(1, "TransportLayer: {}\n", transportLayerName);
			for (size_t i = 0; i < list.size(); ++i) {
				auto& itf = list.at(i);
				if (transportLayerName == itf.transportLayerName())
				{
					print(2, "{:^5} {}\n", i, itf.interfaceDisplayName());
				}
			}
			print("\n");
		}
    }
}

static void list_serials()
{
    ic4::DeviceEnum devEnum;
    auto list = devEnum.enumDevices();

    for (auto&& d : list)
    {
        print("{} ", d.serial());
    }
}

static void print_device( std::string id )
{
    auto dev = find_device( id );
    if( !dev ) {
        throw std::runtime_error( fmt::format( "Failed to find device for id '{}'\n", id ) );
    }

	print("ModelName:     '{}'\n", dev->modelName());
	print("Serial:        '{}'\n", dev->serial());
	print("UserID:        '{}'\n", dev->userID());
	print("UniqueName:    '{}'\n", dev->uniqueName());
	print("DeviceVersion: '{}'\n", dev->version());
	print("InterfaceName: '{}'\n", dev->getInterface().transportLayerName());
}

static void print_interface( std::string id )
{
    auto dev = find_interface( id );
    if( !dev ) {
        throw std::runtime_error( fmt::format( "Failed to find device for id '{}'\n", id ) );
    }

    print( "DisplayName:           '{}'\n", dev->interfaceDisplayName() );
    print( "TransportLayerName:    '{}'\n", dev->transportLayerName() );
    print( "TransportLayerType:    '{}'\n", ic4_helper::toString( dev->transportLayerType() ) );
    print( "TransportLayerVersion: '{}'\n", dev->transportLayerVersion() );
}

template<typename TPropType, class Tprop, class TMethod>
auto fetch_PropertyMethod_value( Tprop& prop, TMethod method_address ) -> std::string
{
    ic4::Error err;
    TPropType v = (prop.*method_address)(err);
    if( err.isError() ) {
        if( err.code() == ic4::ErrorCode::GenICamNotImplemented ) {
            return "n/a";
        }
        return "err";
    }
    return fmt::format( "{}", v );
}

template<class TMethod>
auto fetch_PropertyMethod_value( ic4::PropInteger& prop, TMethod method_address, ic4::PropIntRepresentation int_rep ) -> std::string
{
    ic4::Error err;
    int64_t v = (prop.*method_address)(err);
    if( err.isError() ) {
        if( err.code() == ic4::ErrorCode::GenICamNotImplemented ) {
            return "n/a";
        }
        return "err";
    }
    switch( int_rep )
    {
    case ic4::PropIntRepresentation::Boolean:       return fmt::format( "{}", v != 0 ? 1 : 0 );
    case ic4::PropIntRepresentation::HexNumber:     return fmt::format( "0x{:X}", v );
    case ic4::PropIntRepresentation::IPV4Address:
    {
        uint64_t v0 = (v >> 0) & 0xFF;
        uint64_t v1 = (v >> 8) & 0xFF;
        uint64_t v2 = (v >> 16) & 0xFF;
        uint64_t v3 = (v >> 24) & 0xFF;
        return fmt::format( "{}.{}.{}.{}", v3, v2, v1, v0 );
    }
    case ic4::PropIntRepresentation::MACAddress:
    {
        uint64_t v0 = (v >> 0) & 0xFF;
        uint64_t v1 = (v >> 8) & 0xFF;
        uint64_t v2 = (v >> 16) & 0xFF;
        uint64_t v3 = (v >> 24) & 0xFF;
        uint64_t v4 = (v >> 32) & 0xFF;
        uint64_t v5 = (v >> 40) & 0xFF;
        return fmt::format( "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", v5, v4, v3, v2, v1, v0 );
    }
    case ic4::PropIntRepresentation::Linear:
    case ic4::PropIntRepresentation::Logarithmic:
    case ic4::PropIntRepresentation::PureNumber:
    default:
        return fmt::format( "{}", v );
    }
}

static void    print_property( int offset, const ic4::Property& property )
{
    using namespace ic4_helper;

    auto prop_type = property.type();
    print( offset + 0, "{:24} - Type: {}, DisplayName: {}\n", property.name(), toString( prop_type ), property.displayName() );
    print( offset + 1, "Description: {}\n", property.description() );
    print( offset + 1, "Tooltip: {}\n", property.tooltip() );
    print( offset + 3, "\n" );
    print( offset + 1, "Visibility: {}, Available: {}, Locked: {}, ReadOnly: {}\n", toString( property.visibility() ), property.isAvailable(), property.isLocked(), property.isReadOnly() );

    if( property.isSelector() )
    {
        print( offset + 1, "Selected properties:\n" );
        for( auto&& selected : property.selectedProperties() )
        {
            print( offset + 2, "{}\n", selected.name() );
        }
    }

    switch( prop_type )
    {
    case ic4::PropType::Integer:
    {
        ic4::PropInteger prop = property.asInteger();
        auto inc_mode = prop.incrementMode();
        auto rep = prop.representation();

        print( offset + 1, "Representation: '{}', Unit: '{}', IncrementMode: '{}'\n", toString( rep ), prop.unit(), toString( inc_mode ) );

        if( prop.isAvailable() )
        {
            if( !prop.isReadOnly() ) {
                print( offset + 1, "Min: {}, Max: {}\n",
                    fetch_PropertyMethod_value( prop, &ic4::PropInteger::minimum, rep ),
                    fetch_PropertyMethod_value( prop, &ic4::PropInteger::maximum, rep )
                );
            }
            if( inc_mode == ic4::PropIncrementMode::Increment ) {
                if( !prop.isReadOnly() )
                {
                    print( offset + 1, "Inc: {}\n",
                        fetch_PropertyMethod_value( prop, &ic4::PropInteger::increment, rep )
                    );
                }
            }
            else if( inc_mode == ic4::PropIncrementMode::ValueSet )
            {
                ic4::Error err;
                std::vector<int64_t> vvset = prop.validValueSet(err);
                if( err.isError() ) {
                    print( offset + 1, "Failed to fetch ValidValueSet\n" );
                }
                else
                {
                    print( offset + 1, "ValidValueSet:\n" );
                    for( auto&& val : vvset ) {
                        print( offset + 2, "{}\n", val );
                    }
                    print( "\n" );
                }
            }
            print( offset + 1, "Value: {}\n",
                fetch_PropertyMethod_value( prop, &ic4::PropInteger::getValue, rep )
            );
        }
        break;
    }
    case ic4::PropType::Float:
    {
        ic4::PropFloat prop = property.asFloat();
        auto inc_mode = prop.incrementMode();

        print( offset + 1, "Representation: '{}', Unit: '{}', IncrementMode: '{}', DisplayNotation: {}, DisplayPrecision: {}\n",
            toString( prop.representation() ), prop.unit(), toString( inc_mode ), toString( prop.displayNotation() ), prop.displayPrecision() );

        if( prop.isAvailable() )
        {
            if( !prop.isReadOnly() ) {
                print( offset + 1, "Min: {}, Max: {}\n",
                    fetch_PropertyMethod_value<double>( prop, &ic4::PropFloat::minimum ),
                    fetch_PropertyMethod_value<double>( prop, &ic4::PropFloat::maximum )
                );
            }

            if( inc_mode == ic4::PropIncrementMode::Increment ) {
                print( offset + 1, "Inc: {}\n",
                    fetch_PropertyMethod_value<double>( prop, &ic4::PropFloat::increment )
                );
            }
            else if( inc_mode == ic4::PropIncrementMode::ValueSet )
            {
                ic4::Error err;
                std::vector<double> vvset = prop.validValueSet(err);
                if( err.isError() ) {
                    print( offset + 1, "Failed to fetch ValidValueSet\n" );
                }
                else
                {
                    print( offset + 1, "ValidValueSet:\n" );
                    for( auto&& val : vvset ) {
                        print( offset + 2, "{}\n", val );
                    }
                    print( "\n" );
                }
            }

            print( offset + 1, "Value: {}\n",
                fetch_PropertyMethod_value<double>( prop, &ic4::PropFloat::getValue )
            );
        }
        break;
    }
    case ic4::PropType::Enumeration:
    {
        auto prop = property.asEnumeration();
        print( offset + 1, "EnumEntries:\n" );
        for( auto&& entry : prop.entries( ic4::Error::Ignore() ) )
        {
            auto prop_enum_entry = entry.asEnumEntry();

            print_property( offset + 2, prop_enum_entry );
            print( "\n" );
        }

        if( prop.isAvailable() )
        {
            ic4::Error err;
            auto selected_entry = prop.selectedEntry( err );
            if( err ) {
                print( offset + 1, "Value: {}, SelectedEntry.Name: '{}'\n", "err", "err" );
            }
            else
            {
                print( offset + 1, "Value: {}, SelectedEntry.Name: '{}'\n",
                    fetch_PropertyMethod_value<int64_t>( prop, &ic4::PropEnumeration::getIntValue ),
                    selected_entry.name()
                );
            }
        }
        break;
    }
    case ic4::PropType::Boolean:
    {
        auto prop = property.asBoolean();

        if( prop.isAvailable() ) {
            print( offset + 1, "Value: {}\n",
                fetch_PropertyMethod_value<bool>( prop, &ic4::PropBoolean::getValue )
            );
        }
        break;
    }
    case ic4::PropType::String:
    {
        auto prop = property.asString();

        if( prop.isAvailable() ) {
            print( offset + 1, "Value: '{}', MaxLength: {}\n",
                fetch_PropertyMethod_value<std::string>( prop, &ic4::PropString::getValue ),
                fetch_PropertyMethod_value<uint64_t>( prop, &ic4::PropString::maxLength )
            );
        }
        break;
    }
    case ic4::PropType::Command:
    {
        print( "\n" );
        break;
    }
    case ic4::PropType::Category:
    {
        auto prop = property.asCategory();
        print( offset + 1, "Features:\n" );
        for( auto&& feature : prop.features( ic4::Error::Ignore() ) )
        {
            print( offset + 2, "{}\n", feature.name() );
        }
        break;
    }
    case ic4::PropType::Register:
    {
        ic4::PropRegister prop = property.asRegister();

        print( offset + 1, "Size: {}\n", 
            fetch_PropertyMethod_value<uint64_t>( prop, &ic4::PropRegister::size )
        );
        if( prop.isAvailable() ) {
            ic4::Error err;
            std::vector<uint8_t> vec = prop.getValue( err );
            if( err.isError() ) {
                print( offset + 1, "Value: 'err'" );
            }
            else {
                std::string str;
                size_t max_entries_to_print = 16;
                for( size_t i = 0; i < std::min( max_entries_to_print, vec.size() ); ++i ) {
                    str += fmt::format( "{:x}", vec[i] );
                    str += ", ";
                }
                if( vec.size() > max_entries_to_print ) {
                    str += "...";
                }
                print( offset + 1, "Value: [{}], Value-Size: {}\n", str, vec.size() );
            }
        }
        print( "\n" );
        break;
    }
    case ic4::PropType::Port:
    {
        print( "\n" );
        break;
    }
    case ic4::PropType::EnumEntry:
    {
        ic4::PropEnumEntry prop = property.asEnumEntry();

        if( prop.isAvailable() ) {
            print( offset + 1, "IntValue: {}\n", fetch_PropertyMethod_value<int64_t>( prop, &ic4::PropEnumEntry::intValue ) );
        }
        print( "\n" );
        break;
    }
    default:
        ;
    };
    print( "\n" );
}

static auto split_prop_entry( const std::string& prop_string ) -> std::pair<std::string,std::string>
{
    auto f = prop_string.find( '=' );
    if( f == std::string::npos ) {
        return { prop_string, std::string{} };
    }

    return { prop_string.substr( 0, f ), prop_string.substr( f + 1 ) };
}

static void set_property_from_assign_entry( ic4::PropertyMap& property_map, const std::string& prop_name, const std::string& prop_value )
{
    print( "Setting property '{}' to '{}'\n", prop_name, prop_value );

    ic4::Error err;
    if (!property_map.setValue(prop_name, prop_value, err))
    {
        print("Failed to set value '{}' on property '{}'. Message: {}\n", prop_value, prop_name, err.message());
    }
}

static void print_or_set_PropertyMap_entries( ic4::PropertyMap& map, const std::vector<std::string>& lst )
{
    if( lst.empty() )
    {
        for( auto&& property : map.all() )
        {
            print_property( 0, property );
        }
    }
    else
    {
        for( auto&& entry : lst )
        {
            auto parse_entry = split_prop_entry( entry );
            if( !parse_entry.second.empty() )
            {
                set_property_from_assign_entry( map, parse_entry.first, parse_entry.second );
            }
            else
            {
                auto property = map.find( entry );
                if( property.is_valid() ) {
                    print_property( 0, property );
                } else {
                    print( "Failed to find property for name: '{}'\n", entry );
                }
            }
        }
    }
}

static void exec_prop_cmd( std::string id, bool force_interface, const std::vector<std::string>& lst )
{
    if( force_interface )
    {
        auto dev = find_interface( id );
        if( !dev ) {
            print( "Failed to find interface for id '{}'", id );
            return;
        }
        auto map = dev->interfacePropertyMap();
        print_or_set_PropertyMap_entries( map, lst );
    }
    else
    {
        auto dev = find_device( id );
        if( !dev ) {
            print( "Failed to find device for id '{}'", id );
            return;
        }
        ic4::Grabber g;
        g.deviceOpen( *dev );

        auto map = g.devicePropertyMap();
        print_or_set_PropertyMap_entries( map, lst );
    }
}

static void save_properties( std::string id, bool force_interface, std::string filename )
{
    if( force_interface )
    {
        auto dev = find_interface( id );
        if( !dev ) {
            print( "Failed to find interface for id '{}'", id );
            return;
        }
        auto map = dev->interfacePropertyMap();
        map.serialize( filename );
    }
    else
    {
        auto dev = find_device( id );
        if( !dev ) {
            print( "Failed to find device for id '{}'", id );
            return;
        }
        ic4::Grabber g;
        g.deviceOpen( *dev );

        auto map = g.devicePropertyMap();
        map.serialize( filename );
    }
}

static void save_image( std::string id, std::string filename, int count, int timeout_in_ms, std::string image_type )
{
    auto dev = find_device( id );
    if( !dev ) {
        print( "Failed to find device for id '{}'", id );
        return;
    }
    ic4::Grabber g;
    g.deviceOpen( *dev );

    auto snap_sink = ic4::SnapSink::create();
    g.streamSetup( snap_sink, ic4::StreamSetupOption::AcquisitionStart );

    ic4::Error err;
    auto images = snap_sink->snapSequence( count, timeout_in_ms, err );
    if( err ) {
        if( err.code() == ic4::ErrorCode::Timeout ) {
            print( "Timeout elapsed." );
            // #TODO maybe dissect what to do here.
            return;
        }
        throw err;
    }

    g.acquisitionStop();

    int idx = 0;
    for( auto && image : images )
    {
        std::string actual_filename = filename;
        if( filename.find_first_of( '{' ) != std::string::npos
            && filename.find_first_of( '}' ) != std::string::npos )
        {
            actual_filename = fmt::vformat( filename, fmt::make_format_args( idx ) );
            idx++;
        }
        if( image_type == "bmp" ) {
            ic4::imageBufferSaveAsBitmap( *image, actual_filename, {} );
        }
        else if( image_type == "png" ) {
            ic4::imageBufferSaveAsPng( *image, actual_filename, {} );
        }
        else if( image_type == "tiff" ) {
            ic4::imageBufferSaveAsTiff( *image, actual_filename, {} );
        }
        else if( image_type == "jpeg" ) {
            ic4::imageBufferSaveAsJpeg( *image, actual_filename, {} );
        }
    }
}

#ifdef WIN32

static void show_live( std::string id )
{
    auto dev = find_device( id );
    if( !dev ) {
        print( "Failed to find device for id '{}'", id );
        return;
    }
    ic4::Grabber g;
    g.deviceOpen( *dev );

    auto display = ic4::Display::create( ic4::DisplayType::Default, IC4_WINDOW_HANDLE_NULL );
    g.streamSetup( display, ic4::StreamSetupOption::AcquisitionStart );

    std::mutex mtx;
    bool ended = false;
    std::condition_variable cond;

    display->eventAddWindowClosed( [&]( auto& ) { std::lock_guard<std::mutex> lck{ mtx }; ended = true; cond.notify_all(); } );

    {
        std::unique_lock<std::mutex> lck{ mtx };
        while( !ended ) {
            cond.wait( lck );
        }
    }
    g.acquisitionStop();
}

static void show_prop_page(std::string id, bool id_is_interface, bool show_guru)
{
	ic4gui::PropertyDialogOptions opt = {};
	if (show_guru) {
		opt.initial_visibility = ic4::PropVisibility::Guru;
	}

	if (id_is_interface)
	{
		auto dev = find_interface(id);
		if (!dev) {
			print("Failed to find interface for id '{}'", id);
			return;
		}
		auto map = dev->interfacePropertyMap();
		ic4gui::showPropertyDialog(0, map, opt);
	}
	else
	{
		auto dev = find_device(id);
		if (!dev) {
			print("Failed to find device for id '{}'", id);
			return;
		}
		ic4::Grabber g;
		g.deviceOpen(*dev);

		auto map = g.devicePropertyMap();
		ic4gui::showPropertyDialog(0, map, opt);
	}
}

#endif // WIN32

static void show_version()
{
    auto str = ic4::getVersionInfo();

    if (str.empty())
    {
        print("Unable to retrieve version information.");
        return;
    }

    print("{}", str);

}


static void show_system_info()
{
    auto env_var = helper::get_env_var( "GENICAM_GENTL64_PATH" );
    print( 0, "Environment:\n" );
    print( 1, "GENICAM_GENTL64_PATH: {}\n", env_var );
}

int main( int argc, char** argv )
{
    CLI::App app{ "Simple ic4 camera control utility", "ic4-ctrl"};
    app.set_help_flag();
    app.set_help_all_flag( "-h,--help", "Expand all help" );

    std::string gentl_path = helper::get_env_var( "GENICAM_GENTL64_PATH" );
    app.add_option( "--gentl-path", gentl_path, "GenTL path environment variable to set." )->default_val( gentl_path );

    std::string arg_device_id;
    bool force_interface = false;

    auto list_cmd = app.add_subcommand( "list",
        "List available devices and interfaces by connection."
    );

    auto list_serial = app.add_subcommand( "list-serial",
        "List only serials of available devices."
    );

    auto device_cmd = app.add_subcommand( "device",
        "List devices or a show information for a single device.\n"
        "\tTo list all devices use: `ic4-ctrl device`\n"
        "\tTo show only a specific device: `ic4-ctrl device \"<id>\"\n"
    );
    device_cmd->add_option( "device-id", arg_device_id,
        "If specified only information for this device is printed, otherwise all device are listed. You can specify an index e.g. '0'." );

    auto interface_cmd = app.add_subcommand( "interface",
        "List devices or a show information for a single interface.\n"
        "\tTo list all interfaces: `ic4-ctrl interface`\n"
        "\tTo show only a specific interface: `ic4-ctrl interface \"<id>\"\n"
    );
    interface_cmd->add_option( "interface-id", arg_device_id,
        "If specified only information for this interface is printed, otherwise all interfaces are listed. You can specify an index e.g. '0'." );

    auto props_cmd = app.add_subcommand( "prop",
        "List or set property values of the specified device or interface.\n"
        "\tTo list all device properties 'ic4-ctrl prop <device-id>'.\n"
        "\tTo list specific device properties 'ic4-ctrl prop <device-id> ExposureAuto ExposureTime'.\n"
        "\tTo set specific device properties 'ic4-ctrl prop <device-id> ExposureAuto=Off ExposureTime=0.5'."
    );
    props_cmd->allow_extras();
    props_cmd->add_flag( "--interface", force_interface,
        "If set the <device-id> is interpreted as an interface-id." );
    props_cmd->add_option( "device-id", arg_device_id,
        "Specifies the device to open. You can specify an index e.g. '0'." )->required();

    auto save_props_cmd = app.add_subcommand( "save-prop", 
        "Save properties for the specified device 'ic4-ctrl save-prop -f <filename> <device-id>'." );
    std::string arg_filename;
    save_props_cmd->add_option( "-f,--filename", arg_filename, "Filename to save into." )->required();
    save_props_cmd->add_option( "device-id", arg_device_id,
        "Specifies the device to open. You can specify an index e.g. '0'." )->required();

    auto image_cmd = app.add_subcommand( "image", 
        "Save one or more images from the specified device 'ic4-ctrl image -f <filename> --count 3 --timeout 2000 --type bmp <device-id>'."
    );
    int count = 1;
    int timeout = 1000;
    std::string image_type = "bmp";
    image_cmd->add_option( "-f,--filename", arg_filename, "Filename. Use '{}' to specify where a counter should be placed (e.g. 'test-{}.bmp'." )->required();
    image_cmd->add_option( "--count", count, "Count of frames to capture." )->default_val( count );
    image_cmd->add_option( "--timeout", timeout, "Timeout in milliseconds." )->default_val( timeout );
    image_cmd->add_option( "--type", image_type, "Image file type to save. [bmp,png,jpeg,tiff]" )->default_val( image_type );
    image_cmd->add_option( "device-id", arg_device_id,
        "Specifies the device to open. You can specify an index e.g. '0'." )->required();
#ifdef WIN32

    auto live_cmd = app.add_subcommand( "live", "Display a live stream. 'ic4-ctrl live <device-id>'." );
    live_cmd->add_option( "device-id", arg_device_id,
        "Specifies the device to open. You can specify an index e.g. '0'." )->required();

	bool show_default_guru = false;
    auto show_prop_page_cmd = app.add_subcommand( "show-prop", "Display the property page for the device or interface id. 'ic4-ctrl show-prop <id>'." );
    show_prop_page_cmd->add_option( "device-id", arg_device_id,
        "Specifies the device to open. You can specify an index e.g. '0'." )->required();
    show_prop_page_cmd->add_flag( "--interface", force_interface,
        "If set the <device-id> is interpreted as an interface-id." );
	show_prop_page_cmd->add_flag("-g,--guru", show_default_guru,
		"Start the dialog with Visibility set to ic4::PropVisibility::Guru.");

#endif // WIN32

    auto system_cmd = app.add_subcommand( "system",
        "List some information for about the system."
    );

    auto version_cmd = app.add_subcommand( "version",
        "List version information about IC4."
    );

    try
    {
        app.parse( argc, argv );
    }
    catch( const CLI::ParseError& e )
    {
        return app.exit( e );
    }

    if( !gentl_path.empty() ) {
        helper::set_env_var( "GENICAM_GENTL64_PATH", gentl_path );
    }

    ic4::InitLibraryConfig config =
    {
        ic4::ErrorHandlerBehavior::Throw,
        ic4::LogLevel::Off
    };
    ic4::initLibrary(config);

    try
    {
        if( list_cmd->parsed() )
        {
			list_all_by_connection();
        }
        else if ( list_serial->parsed() )
        {
            list_serials();
        }
        else if( device_cmd->parsed() )
        {
            if( arg_device_id.empty() ) {
                list_devices();
            } else {
                print_device( arg_device_id );
            }
        }
        else if( interface_cmd->parsed() )
        {
            if( arg_device_id.empty() ) {
                list_interfaces();
            } else {
                print_interface( arg_device_id );
            }
        }
        else if( props_cmd->parsed() )
        {
            exec_prop_cmd( arg_device_id, force_interface, props_cmd->remaining() );
        }
        else if( save_props_cmd->parsed() )
        {
            save_properties( arg_device_id, force_interface, arg_filename );
        }
        else if( image_cmd->parsed() ) {
            save_image( arg_device_id, arg_filename, count, timeout, image_type );
        }
#ifdef WIN32
        else if( live_cmd->parsed() )
        {
            show_live(arg_device_id);
        }
        else if( show_prop_page_cmd->parsed() )
        {
            show_prop_page(arg_device_id, force_interface, show_default_guru);
        }
#endif // WIN32
        else if( system_cmd->parsed() )
        {
            show_system_info();
        }
        else if( version_cmd->parsed() )
        {
            show_version();
        }
        else
        {
            print("No arguments given\n\n");
            print("{}\n", app.get_formatter()->make_help(&app, app.get_name(), CLI::AppFormatMode::All));
        }
    }
    catch( const std::exception& ex )
    {
        fmt::print( stderr, "Error: {}\n", ex.what() );
    }

	ic4::exitLibrary();

	return 0;
}
