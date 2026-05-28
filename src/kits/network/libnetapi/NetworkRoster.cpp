/*
 * Copyright 2010, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 */


#include <NetworkRoster.h>

#include <errno.h>
#include <set>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#endif

#include <NetworkDevice.h>
#include <NetworkInterface.h>

//#include <net_notifications.h>
#include <AutoDeleter.h>
#include <NetServer.h>


// TODO: using AF_INET for the socket isn't really a smart idea, as one
// could completely remove IPv4 support from the stack easily.
// Since in the stack, device_interfaces are pretty much interfaces now, we
// could get rid of them more or less, and make AF_LINK provide the same
// information as AF_INET for the interface functions mostly.


BNetworkRoster BNetworkRoster::sDefault;


/*static*/ BNetworkRoster&
BNetworkRoster::Default()
{
	return sDefault;
}


size_t
BNetworkRoster::CountInterfaces() const
{
#ifdef _WIN32
	ULONG bufferSize = 16 * 1024;
	IP_ADAPTER_ADDRESSES* addresses
		= (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
	if (addresses == NULL)
		return 0;

	ULONG result = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses,
		&bufferSize);
	if (result == ERROR_BUFFER_OVERFLOW) {
		free(addresses);
		addresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
		if (addresses == NULL)
			return 0;
		result = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses,
			&bufferSize);
	}

	if (result != NO_ERROR) {
		free(addresses);
		return 0;
	}

	size_t count = 0;
	for (IP_ADAPTER_ADDRESSES* current = addresses; current != NULL;
			current = current->Next) {
		if (current->AdapterName != NULL && current->AdapterName[0] != '\0')
			count++;
	}

	free(addresses);
	return count;
#else
	ifaddrs* list = NULL;
	if (getifaddrs(&list) != 0)
		return 0;

	std::set<std::string> names;
	for (ifaddrs* current = list; current != NULL; current = current->ifa_next) {
		if (current->ifa_name != NULL && current->ifa_name[0] != '\0')
			names.insert(current->ifa_name);
	}

	freeifaddrs(list);
	return names.size();
#endif
}


status_t
BNetworkRoster::GetNextInterface(uint32* cookie,
	BNetworkInterface& interface) const
{
	if (cookie == NULL)
		return B_BAD_VALUE;

#ifdef _WIN32
	ULONG bufferSize = 16 * 1024;
	IP_ADAPTER_ADDRESSES* addresses
		= (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
	if (addresses == NULL)
		return B_NO_MEMORY;

	ULONG result = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses,
		&bufferSize);
	if (result == ERROR_BUFFER_OVERFLOW) {
		free(addresses);
		addresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);
		if (addresses == NULL)
			return B_NO_MEMORY;
		result = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addresses,
			&bufferSize);
	}

	if (result != NO_ERROR) {
		free(addresses);
		return B_ERROR;
	}

	for (uint32 index = 0, seen = 0; ; index++) {
		IP_ADAPTER_ADDRESSES* current = addresses;
		for (uint32 i = 0; i < index && current != NULL; i++)
			current = current->Next;

		if (current == NULL)
			break;
		if (current->AdapterName == NULL || current->AdapterName[0] == '\0')
			continue;

		if (seen == *cookie) {
			interface.SetTo(current->AdapterName);
			(*cookie)++;
			free(addresses);
			return B_OK;
		}
		seen++;
	}

	free(addresses);
	return B_BAD_VALUE;
#else
	ifaddrs* list = NULL;
	if (getifaddrs(&list) != 0)
		return errno;

	std::set<std::string> names;
	for (ifaddrs* current = list; current != NULL; current = current->ifa_next) {
		if (current->ifa_name != NULL && current->ifa_name[0] != '\0')
			names.insert(current->ifa_name);
	}

	uint32 index = 0;
	for (std::set<std::string>::const_iterator it = names.begin();
			it != names.end(); ++it, ++index) {
		if (index == *cookie) {
			interface.SetTo(it->c_str());
			(*cookie)++;
			freeifaddrs(list);
			return B_OK;
		}
	}

	freeifaddrs(list);
	return B_BAD_VALUE;
#endif
}

#if 0
status_t
BNetworkRoster::AddInterface(const char* name)
{
	FileDescriptorCloser socket (::socket(AF_INET, SOCK_DGRAM, 0));
	if (!socket.IsSet())
		return errno;

	ifaliasreq request;
	memset(&request, 0, sizeof(ifaliasreq));
	strlcpy(request.ifra_name, name, IF_NAMESIZE);

	if (ioctl(socket.Get(), SIOCAIFADDR, &request, sizeof(request)) != 0)
		return errno;

	return B_OK;
}


status_t
BNetworkRoster::AddInterface(const BNetworkInterface& interface)
{
	return AddInterface(interface.Name());
}


status_t
BNetworkRoster::RemoveInterface(const char* name)
{
	FileDescriptorCloser socket(::socket(AF_INET, SOCK_DGRAM, 0));
	if (!socket.IsSet())
		return errno;

	ifreq request;
	strlcpy(request.ifr_name, name, IF_NAMESIZE);

	request.ifr_addr.sa_family = AF_UNSPEC;

	if (ioctl(socket.Get(), SIOCDIFADDR, &request, sizeof(request)) != 0)
		return errno;

	return B_OK;
}


status_t
BNetworkRoster::RemoveInterface(const BNetworkInterface& interface)
{
	return RemoveInterface(interface.Name());
}


int32
BNetworkRoster::CountPersistentNetworks() const
{
	BMessenger networkServer(kNetServerSignature);
	BMessage message(kMsgCountPersistentNetworks);
	BMessage reply;
	if (networkServer.SendMessage(&message, &reply) != B_OK)
		return 0;

	int32 count = 0;
	if (reply.FindInt32("count", &count) != B_OK)
		return 0;

	return count;
}


status_t
BNetworkRoster::GetNextPersistentNetwork(uint32* cookie,
	wireless_network& network) const
{
	BMessenger networkServer(kNetServerSignature);
	BMessage message(kMsgGetPersistentNetwork);
	message.AddInt32("index", (int32)*cookie);

	BMessage reply;
	status_t result = networkServer.SendMessage(&message, &reply);
	if (result != B_OK)
		return result;

	status_t status;
	if (reply.FindInt32("status", &status) != B_OK)
		return B_ERROR;
	if (status != B_OK)
		return status;

	BMessage networkMessage;
	if (reply.FindMessage("network", &networkMessage) != B_OK)
		return B_ERROR;

	BString networkName;
	if (networkMessage.FindString("name", &networkName) != B_OK)
		return B_ERROR;

	memset(network.name, 0, sizeof(network.name));
	strlcpy(network.name, networkName.String(), sizeof(network.name));

	BNetworkAddress address;
	if (networkMessage.FindFlat("address", &network.address) != B_OK)
		network.address.Unset();

	if (networkMessage.FindUInt32("flags", &network.flags) != B_OK)
		network.flags = 0;

	if (networkMessage.FindUInt32("authentication_mode",
			&network.authentication_mode) != B_OK) {
		network.authentication_mode = B_NETWORK_AUTHENTICATION_NONE;
	}

	if (networkMessage.FindUInt32("cipher", &network.cipher) != B_OK)
		network.cipher = B_NETWORK_CIPHER_NONE;

	if (networkMessage.FindUInt32("group_cipher", &network.group_cipher)
			!= B_OK) {
		network.group_cipher = B_NETWORK_CIPHER_NONE;
	}

	if (networkMessage.FindUInt32("key_mode", &network.key_mode) != B_OK)
		network.key_mode = B_KEY_MODE_NONE;

	return B_OK;
}


status_t
BNetworkRoster::AddPersistentNetwork(const wireless_network& network)
{
	BMessage message(kMsgAddPersistentNetwork);
	BString networkName;
	networkName.SetTo(network.name, sizeof(network.name));
	status_t status = message.AddString("name", networkName);
	if (status == B_OK) {
		BNetworkAddress address = network.address;
		status = message.AddFlat("address", &address);
	}

	if (status == B_OK)
		status = message.AddUInt32("flags", network.flags);
	if (status == B_OK) {
		status = message.AddUInt32("authentication_mode",
			network.authentication_mode);
	}
	if (status == B_OK)
		status = message.AddUInt32("cipher", network.cipher);
	if (status == B_OK)
		status = message.AddUInt32("group_cipher", network.group_cipher);
	if (status == B_OK)
		status = message.AddUInt32("key_mode", network.key_mode);

	if (status != B_OK)
		return status;

	BMessenger networkServer(kNetServerSignature);
	BMessage reply;
	status = networkServer.SendMessage(&message, &reply);
	if (status == B_OK)
		reply.FindInt32("status", &status);

	return status;
}


status_t
BNetworkRoster::RemovePersistentNetwork(const char* name)
{
	BMessage message(kMsgRemovePersistentNetwork);
	status_t status = message.AddString("name", name);
	if (status != B_OK)
		return status;

	BMessenger networkServer(kNetServerSignature);
	BMessage reply;
	status = networkServer.SendMessage(&message, &reply);
	if (status == B_OK)
		reply.FindInt32("status", &status);

	return status;
}

status_t
BNetworkRoster::StartWatching(const BMessenger& target, uint32 eventMask)
{
	return start_watching_network(eventMask, target);
}


void
BNetworkRoster::StopWatching(const BMessenger& target)
{
	stop_watching_network(target);
}
#endif

// #pragma mark - private


BNetworkRoster::BNetworkRoster()
{
}


BNetworkRoster::~BNetworkRoster()
{
}
