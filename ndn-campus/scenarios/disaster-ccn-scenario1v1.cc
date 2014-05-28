/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * (c) 2009, GTech Systems, Inc. - Alfred Park <park@gtech-systems.com>
 *
 * DARPA NMS Campus Network Model
 *
 * This topology replicates the original NMS Campus Network model
 * with the exception of chord links (which were never utilized in the
 * original model)
 * Link Bandwidths and Delays may not be the same as the original
 * specifications
 *
 * The fundamental unit of the NMS model consists of a campus network. The
 * campus network topology can been seen here:
 * http://www.nsnam.org/~jpelkey3/nms.png
 * The number of hosts (default 42) is variable.  Finally, an arbitrary
 * number of these campus networks can be connected together (default 2)
 * to make very large simulations.
 */

// for timing functions
#include <cstdlib>
#include <sys/time.h>
#include <fstream>

// Standard C++ modules
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/time.h>
#include <vector>
#include <iostream>

// Random modules
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/random/variate_generator.hpp>

// ns3 modules
#include <ns3-dev/ns3/applications-module.h>
#include <ns3-dev/ns3/core-module.h>
#include <ns3-dev/ns3/internet-module.h>
#include <ns3-dev/ns3/ipv4-address.h>
#include <ns3-dev/ns3/ipv4-interface-address.h>
#include <ns3-dev/ns3/ipv4-list-routing-helper.h>
#include <ns3-dev/ns3/ipv4-nix-vector-helper.h>
#include <ns3-dev/ns3/ipv4-static-routing-helper.h>
#include <ns3-dev/ns3/network-module.h>
#include <ns3-dev/ns3/onoff-application.h>
#include <ns3-dev/ns3/packet-sink.h>
#include <ns3-dev/ns3/point-to-point-module.h>
#include <ns3-dev/ns3/simulator.h>
// ndnSIM modules
#include <ns3-dev/ns3/ndnSIM-module.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-rate-l3-tracer.h>
#include <ns3-dev/ns3/ndnSIM/utils/tracers/ipv4-seqs-app-tracer.h>

using namespace ns3;
using namespace boost;
using namespace std;

namespace br = boost::random;

typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec*1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1)-TIMER_SECONDS (_t2))

NS_LOG_COMPONENT_DEFINE ("CampusNetworkModel");

// Number generator
br::mt19937_64 gen;

// Obtains a random number from a uniform distribution between min and max.
// Must seed number generator to ensure randomness at runtime.
int obtain_Num(int min, int max) {
    br::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

// Obtains a random list of clients and servers. Must be run once all nodes have been
// created to function correctly
tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > assignClientsandServers(int num_clients, int num_servers) {

	char buffer[250];

	// Obtain the global list of Nodes in the simulation
	NodeContainer global = NodeContainer::GetGlobal ();

	// Get the number of nodes in the simulation
	uint32_t size = global.GetN ();

	sprintf(buffer, "assignClientsandServers, we have %d nodes", size);

	NS_LOG_INFO (buffer);

	sprintf(buffer, "assignClientsandServers, %d clients, %d servers", num_clients, num_servers);

	// Check that we haven't asked for a scenario where we don't have enough Nodes to fufill
	// the requirements
	if (num_clients + num_servers > size) {
		return tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > ();
	}

	std::vector<Ptr<Node> > globalmutable;

	// Copy the global list into a mutable vector
	for (uint32_t i = 0; i < size; i++) {
		globalmutable.push_back (global.Get(i));
	}

	uint32_t clientMin = size - num_clients - 1;
	uint32_t serverMin = clientMin - num_servers;

	std::vector<Ptr<Node> > ClientContainer;
	std::vector<Ptr<Node> > ServerContainer;

	// Apply Fisher-Yates shuffle - start with clients
	for (uint32_t i = size-1; i > clientMin; i--) {
		// Get a random number
		int toSwap = obtain_Num (0, i);
		// Push into the client container the number we got from the global vector
		ClientContainer.push_back (globalmutable[toSwap]);
		// Swap the obtained number with the last element
		std::swap (globalmutable[toSwap], globalmutable[i]);
	}

	// Apply Fisher-Yates shuffle - servers
	for (uint32_t i = clientMin; i > serverMin; i--) {
		// Get a random number
		int toSwap = obtain_Num(0, i);
		// Push into the client container the number we got from the global vector
		ServerContainer.push_back(globalmutable[toSwap]);
		// Swap the obtained number with the last element
		std::swap (globalmutable[toSwap], globalmutable[i]);
	}

	return tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > (ClientContainer,ServerContainer);
}

void Progress ()
{
	Simulator::Schedule (Seconds (0.1), Progress);
}

template <typename T>
class Array2D
{
public:
	Array2D (const size_t x, const size_t y) : p (new T*[x]), m_xMax (x)
	{
		for (size_t i = 0; i < m_xMax; i++)
			p[i] = new T[y];
	}

	~Array2D (void)
	{
		for (size_t i = 0; i < m_xMax; i++)
			delete[] p[i];
		delete p;
		p = 0;
	}

	T* operator[] (const size_t i)
	{
		return p[i];
	}

private:
	T** p;
	const size_t m_xMax;
};

template <typename T>
class Array3D
{
public:
	Array3D (const size_t x, const size_t y, const size_t z) : p (new Array2D<T>*[x]), m_xMax (x)
	{
		for (size_t i = 0; i < m_xMax; i++)
			p[i] = new Array2D<T> (y, z);
	}

	~Array3D (void)
	{
		for (size_t i = 0; i < m_xMax; i++)
		{
			delete p[i];
			p[i] = 0;
		}
		delete[] p;
		p = 0;
	}

	Array2D<T>& operator[] (const size_t i)
	{
		return *(p[i]);
	}

private:
	Array2D<T>** p;
	const size_t m_xMax;
};

int main (int argc, char *argv[])
{
	TIMER_TYPE t0, t1, t2;
	TIMER_NOW (t0);
	std::cout << " ==== DARPA NMS CAMPUS NETWORK SIMULATION ====" << std::endl;

	// These are our scenario arguments
	uint32_t contentsize = 1048576; // Size in bytes of the content to transfer
	uint32_t clients = 300; // Number of clients in the network
	uint32_t servers = 2; // Number of servers in the network
	uint32_t networks = 2; // Number of additional nodes in the network

	char results[250] = "results";

	int nCN = 3, nLANClients = 42; 
	bool nix = true;
	
	// Char array for output strings
	char buffer[250];


	
	CommandLine cmd;
	cmd.AddValue ("CN", "Number of total CNs [2]", nCN);
	cmd.AddValue ("LAN", "Number of nodes per LAN [42]", nLANClients);
	cmd.AddValue ("NIX", "Toggle nix-vector routing", nix);
	cmd.AddValue ("contentsize",
				"Total number of bytes for application to send", contentsize);
	cmd.AddValue ("clients", "Total number of clients in the network", clients);
	cmd.AddValue ("servers", "Total number of servers in the network", servers);
	cmd.AddValue ("networks", "Number of networks in the simulation", networks);
	cmd.Parse (argc,argv);
	/*if (nCN < 2)
	{
		std::cout << "Number of total CNs (" << nCN << ") lower than minimum of 2"
				<< std::endl;
		return 1;
	}*/
    
		
	// Overwrite nCN with networks
    nCN = networks;
	
    std::cout << "Number of CNs: " << nCN << ", LAN nodes: " << nLANClients << std::endl;

	Array2D<NodeContainer> nodes_net0(nCN, 3);
	Array2D<NodeContainer> nodes_net1(nCN, 6);
	NodeContainer* nodes_netLR = new NodeContainer[nCN];
	Array2D<NodeContainer> nodes_net2(nCN, 14);
	Array3D<NodeContainer> nodes_net2LAN(nCN, 7, nLANClients);
	Array2D<NodeContainer> nodes_net3(nCN, 9);
	Array3D<NodeContainer> nodes_net3LAN(nCN, 5, nLANClients);

	PointToPointHelper p2p_2gb200ms, p2p_1gb5ms, p2p_100mb1ms;
	InternetStackHelper stack;
	Ipv4InterfaceContainer ifs;
	Array2D<Ipv4InterfaceContainer> ifs0(nCN, 3);
	Array2D<Ipv4InterfaceContainer> ifs1(nCN, 6);
	Array2D<Ipv4InterfaceContainer> ifs2(nCN, 14);
	Array2D<Ipv4InterfaceContainer> ifs3(nCN, 9);
	Array3D<Ipv4InterfaceContainer> ifs2LAN(nCN, 7, nLANClients);
	Array3D<Ipv4InterfaceContainer> ifs3LAN(nCN, 5, nLANClients);

	Ipv4AddressHelper address;
	std::ostringstream oss;
	p2p_1gb5ms.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
	p2p_1gb5ms.SetChannelAttribute ("Delay", StringValue ("5ms"));
	p2p_2gb200ms.SetDeviceAttribute ("DataRate", StringValue ("2Gbps"));
	p2p_2gb200ms.SetChannelAttribute ("Delay", StringValue ("200ms"));
	p2p_100mb1ms.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
	p2p_100mb1ms.SetChannelAttribute ("Delay", StringValue ("1ms"));

	// Setup NixVector Routing
	Ipv4NixVectorHelper nixRouting;
	Ipv4StaticRoutingHelper staticRouting;

	Ipv4ListRoutingHelper list;
	list.Add (staticRouting, 0);
	list.Add (nixRouting, 10);

	if (nix)
	{
		stack.SetRoutingHelper (list); // has effect on the next Install ()
	}

	// Create Campus Networks
	for (int z = 0; z < nCN; ++z)
	{
		std::cout << "Creating Campus Network " << z << ":" << std::endl;
		// Create Net0
		std::cout << "  SubNet [ 0"; //0
		for (int i = 0; i < 3; ++i)
		{
			nodes_net0[z][i].Create (1);
			stack.Install (nodes_net0[z][i]);
		}

		nodes_net0[z][0].Add (nodes_net0[z][1].Get (0));
		nodes_net0[z][1].Add (nodes_net0[z][2].Get (0));
		nodes_net0[z][2].Add (nodes_net0[z][0].Get (0));
		NetDeviceContainer ndc0[3];

		for (int i = 0; i < 3; ++i)
		{
			ndc0[i] = p2p_1gb5ms.Install (nodes_net0[z][i]);
		}

		// Create Net1
		std::cout << " 1";//1
		for (int i = 0; i < 6; ++i)
		{
			nodes_net1[z][i].Create (1);
			stack.Install(nodes_net1[z][i]);
		}

		nodes_net1[z][0].Add (nodes_net1[z][1].Get (0));
		nodes_net1[z][2].Add (nodes_net1[z][0].Get (0));
		nodes_net1[z][3].Add (nodes_net1[z][0].Get (0));
		nodes_net1[z][4].Add (nodes_net1[z][1].Get (0));
		nodes_net1[z][5].Add (nodes_net1[z][1].Get (0));
		NetDeviceContainer ndc1[6];

		for (int i = 0; i < 6; ++i)
		{
			if (i == 1)
			{
				continue;
			}

			ndc1[i] = p2p_1gb5ms.Install (nodes_net1[z][i]);
		}

		// Connect Net0 <-> Net1
		NodeContainer net0_1;
		net0_1.Add (nodes_net0[z][2].Get (0));
		net0_1.Add (nodes_net1[z][0].Get (0));
		NetDeviceContainer ndc0_1;
		ndc0_1 = p2p_1gb5ms.Install(net0_1);
		oss.str ("");
		oss << 10 + z << ".1.252.0";
		address.SetBase(oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign(ndc0_1);

		// Create Net2
		std::cout << " 2";//2
		for (int i = 0; i < 14; ++i)
		{
			nodes_net2[z][i].Create (1);
			stack.Install(nodes_net2[z][i]);
		}

		nodes_net2[z][0].Add (nodes_net2[z][1].Get (0));
		nodes_net2[z][2].Add (nodes_net2[z][0].Get (0));
		nodes_net2[z][1].Add (nodes_net2[z][3].Get (0));
		nodes_net2[z][3].Add (nodes_net2[z][2].Get (0));
		nodes_net2[z][4].Add (nodes_net2[z][2].Get (0));
		nodes_net2[z][5].Add (nodes_net2[z][3].Get (0));
		nodes_net2[z][6].Add (nodes_net2[z][5].Get (0));
		nodes_net2[z][7].Add (nodes_net2[z][2].Get (0));
		nodes_net2[z][8].Add (nodes_net2[z][3].Get (0));
		nodes_net2[z][9].Add (nodes_net2[z][4].Get (0));
		nodes_net2[z][10].Add (nodes_net2[z][5].Get (0));
		nodes_net2[z][11].Add (nodes_net2[z][6].Get (0));
		nodes_net2[z][12].Add (nodes_net2[z][6].Get (0));
		nodes_net2[z][13].Add (nodes_net2[z][6].Get (0));
		NetDeviceContainer ndc2[14];

		for (int i = 0; i < 14; ++i)
		{
			ndc2[i] = p2p_1gb5ms.Install (nodes_net2[z][i]);
		}
		///      NetDeviceContainer ndc2LAN[7][nLANClients];

		Array2D<NetDeviceContainer> ndc2LAN(7, nLANClients);
		for (int i = 0; i < 7; ++i)
		{
			oss.str ("");
			oss << 10 + z << ".4." << 15 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			for (int j = 0; j < nLANClients; ++j)
			{
				nodes_net2LAN[z][i][j].Create (1);
				stack.Install (nodes_net2LAN[z][i][j]);
				nodes_net2LAN[z][i][j].Add (nodes_net2[z][i+7].Get (0));
				ndc2LAN[i][j] = p2p_100mb1ms.Install (nodes_net2LAN[z][i][j]);
				ifs2LAN[z][i][j] = address.Assign (ndc2LAN[i][j]);
			}
		}

		// Create Net3
		std::cout << " 3 ]" << std::endl;//3
		for (int i = 0; i < 9; ++i)
		{
			nodes_net3[z][i].Create (1);
			stack.Install (nodes_net3[z][i]);
		}

		nodes_net3[z][0].Add (nodes_net3[z][1].Get (0));
		nodes_net3[z][1].Add (nodes_net3[z][2].Get (0));
		nodes_net3[z][2].Add (nodes_net3[z][3].Get (0));
		nodes_net3[z][3].Add (nodes_net3[z][1].Get (0));
		nodes_net3[z][4].Add (nodes_net3[z][0].Get (0));
		nodes_net3[z][5].Add (nodes_net3[z][0].Get (0));
		nodes_net3[z][6].Add (nodes_net3[z][2].Get (0));
		nodes_net3[z][7].Add (nodes_net3[z][3].Get (0));
		nodes_net3[z][8].Add (nodes_net3[z][3].Get (0));
		NetDeviceContainer ndc3[9];
		for (int i = 0; i < 9; ++i)
		{
			ndc3[i] = p2p_1gb5ms.Install (nodes_net3[z][i]);
		}
		///      NetDeviceContainer ndc3LAN[5][nLANClients];
		Array2D<NetDeviceContainer> ndc3LAN(5, nLANClients);
		for (int i = 0; i < 5; ++i)
		{
			oss.str ("");
			oss << 10 + z << ".5." << 10 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.255");
			for (int j = 0; j < nLANClients; ++j)
			{
				nodes_net3LAN[z][i][j].Create (1);
				stack.Install (nodes_net3LAN[z][i][j]);
				nodes_net3LAN[z][i][j].Add (nodes_net3[z][i+4].Get (0));
				ndc3LAN[i][j] = p2p_100mb1ms.Install (nodes_net3LAN[z][i][j]);
				ifs3LAN[z][i][j] = address.Assign (ndc3LAN[i][j]);
			}
		}

		std::cout << "  Connecting Subnets..." << std::endl;
		// Create Lone Routers (Node 4 & 5)
		nodes_netLR[z].Create (2);
		stack.Install (nodes_netLR[z]);
		NetDeviceContainer ndcLR;
		ndcLR = p2p_1gb5ms.Install (nodes_netLR[z]);
		// Connect Net2/Net3 through Lone Routers to Net0
		NodeContainer net0_4, net0_5, net2_4a, net2_4b, net3_5a, net3_5b;
		net0_4.Add (nodes_netLR[z].Get (0));
		net0_4.Add (nodes_net0[z][0].Get (0));
		net0_5.Add (nodes_netLR[z].Get  (1));
		net0_5.Add (nodes_net0[z][1].Get (0));
		net2_4a.Add (nodes_netLR[z].Get (0));
		net2_4a.Add (nodes_net2[z][0].Get (0));
		net2_4b.Add (nodes_netLR[z].Get (1));
		net2_4b.Add (nodes_net2[z][1].Get (0));
		net3_5a.Add (nodes_netLR[z].Get (1));
		net3_5a.Add (nodes_net3[z][0].Get (0));
		net3_5b.Add (nodes_netLR[z].Get (1));
		net3_5b.Add (nodes_net3[z][1].Get (0));
		NetDeviceContainer ndc0_4, ndc0_5, ndc2_4a, ndc2_4b, ndc3_5a, ndc3_5b;
		ndc0_4 = p2p_1gb5ms.Install (net0_4);
		oss.str ("");
		oss << 10 + z << ".1.253.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc0_4);
		ndc0_5 = p2p_1gb5ms.Install (net0_5);
		oss.str ("");
		oss << 10 + z << ".1.254.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc0_5);
		ndc2_4a = p2p_1gb5ms.Install (net2_4a);
		oss.str ("");
		oss << 10 + z << ".4.253.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc2_4a);
		ndc2_4b = p2p_1gb5ms.Install (net2_4b);
		oss.str ("");
		oss << 10 + z << ".4.254.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc2_4b);
		ndc3_5a = p2p_1gb5ms.Install (net3_5a);
		oss.str ("");
		oss << 10 + z << ".5.253.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc3_5a);
		ndc3_5b = p2p_1gb5ms.Install (net3_5b);
		oss.str ("");
		oss << 10 + z << ".5.254.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndc3_5b);
		// Assign IP addresses
		std::cout << "  Assigning IP addresses..." << std::endl;

		for (int i = 0; i < 3; ++i)
		{
			oss.str ("");
			oss << 10 + z << ".1." << 1 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			ifs0[z][i] = address.Assign (ndc0[i]);
		}

		for (int i = 0; i < 6; ++i)
		{
			if (i == 1)
			{
				continue;
			}
			oss.str ("");
			oss << 10 + z << ".2." << 1 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			ifs1[z][i] = address.Assign (ndc1[i]);
		}

		oss.str ("");
		oss << 10 + z << ".3.1.0";
		address.SetBase (oss.str ().c_str (), "255.255.255.0");
		ifs = address.Assign (ndcLR);

		for (int i = 0; i < 14; ++i)
		{
			oss.str ("");
			oss << 10 + z << ".4." << 1 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			ifs2[z][i] = address.Assign (ndc2[i]);
		}

		for (int i = 0; i < 9; ++i)
		{
			oss.str ("");
			oss << 10 + z << ".5." << 1 + i << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			ifs3[z][i] = address.Assign (ndc3[i]);
		}
	}
	// Create Ring Links
	if (nCN > 1)
	{
		std::cout << "Forming Ring Topology..." << std::endl;
		NodeContainer* nodes_ring = new NodeContainer[nCN];
		for (int z = 0; z < nCN-1; ++z)
		{
			nodes_ring[z].Add (nodes_net0[z][0].Get (0));
			nodes_ring[z].Add (nodes_net0[z+1][0].Get (0));
		}
		nodes_ring[nCN-1].Add (nodes_net0[nCN-1][0].Get (0));
		nodes_ring[nCN-1].Add (nodes_net0[0][0].Get (0));
		NetDeviceContainer* ndc_ring = new NetDeviceContainer[nCN];
		for (int z = 0; z < nCN; ++z)
		{
			ndc_ring[z] = p2p_2gb200ms.Install (nodes_ring[z]);
			oss.str ("");
			oss << "254.1." << z + 1 << ".0";
			address.SetBase (oss.str ().c_str (), "255.255.255.0");
			ifs = address.Assign (ndc_ring[z]);
		}
		delete[] ndc_ring;
		delete[] nodes_ring;
	}

	// Make sure to seed our random
	gen.seed(std::time(0));
	
	// With the network assigned, time to randomly obtain clients and servers
	NS_LOG_INFO ("Obtaining the clients and servers");

	// Obtain the random lists of server and clients
    // fixed location for server in scenario1
        //tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > t = assignClientsandServers(clients, servers);	
    tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > t = assignClientsandServers(clients, 0);


	// Separate the tuple into clients and servers
	std::vector<Ptr<Node> > clientVector = t.get<0> ();
	std::vector<Ptr<Node> > serverVector = t.get<1> ();

	NodeContainer clientNodes;
	std::vector<uint32_t> clientNodeIds;
	NodeContainer serverNodes;
	std::vector<uint32_t> serverNodeIds;
	
	// We have to manually introduce the Ptr<Node> to the NodeContainers
		// We do this to make them easier to control later

	
	//consumerHelper.SetPrefix ("test");
	//ApplicationContainer apps;
	//std::string prefix = "results/congestion-pop-run-";
	
	ndn::StackHelper ndnHelper;
	
    // Install Content Store
	ndnHelper.SetContentStore("ns3::ndn::cs::Freshness::Lru","MaxSize","3072");// 30% of whole contents
	//Set forwarding strategy
	ndnHelper.SetForwardingStrategy ("ns3::ndn::fw::BestRoute");

	ndnHelper.InstallAll ();
	
	ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.InstallAll ();
	if (networks == 1){
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/wasedau/net1/server/", nodes_net1[0][5].Get (0));
	}
	else if (networks == 2){
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/wasedau/net1/server/", nodes_net1[0][5].Get (0));
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/nishiwasedau/net1/server/", nodes_net1[1][5].Get (0));
	}
	else if(networks == 3){
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/wasedau/net1/server/", nodes_net1[0][5].Get (0));
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/nishiwasedau/net1/server/", nodes_net1[1][5].Get (0));
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/toyamawasedau/net1/server/", nodes_net1[2][5].Get (0));
	}
	else{
		cout<< "Too many networks, bro!"<< endl;
	}

	ndn::GlobalRoutingHelper::CalculateRoutes ();

	
	//ApplicationContainer apps;
	ndn::AppHelper producerHelper ("ns3::ndn::Producer");
	srand((int)time(NULL)); 
    
    // server NodeContainer
    if (servers == 1){
        Ptr<Node> server_tmp = nodes_net1[0][5].Get (0);
        uint32_t server_nodeNum = server_tmp->GetId();
	
        serverNodes.Add(server_tmp);
	    serverNodeIds.push_back(server_nodeNum);
    }
    else if (servers == 2){
            Ptr<Node> server_tmp = nodes_net1[0][5].Get (0);
            uint32_t server_nodeNum = server_tmp->GetId();
            serverNodes.Add(server_tmp);
	        serverNodeIds.push_back(server_nodeNum);

            Ptr<Node> server_tmp1 = nodes_net1[1][5].Get (0);
            uint32_t server_nodeNum1 = server_tmp1->GetId();
            serverNodes.Add(server_tmp1);
	        serverNodeIds.push_back(server_nodeNum1);
        }
    else if(servers == 3){
            Ptr<Node> server_tmp = nodes_net1[0][5].Get (0);
            uint32_t server_nodeNum = server_tmp->GetId();
            serverNodes.Add(server_tmp);
	        serverNodeIds.push_back(server_nodeNum);

            Ptr<Node> server_tmp1 = nodes_net1[1][5].Get (0);
            uint32_t server_nodeNum1 = server_tmp1->GetId();
            serverNodes.Add(server_tmp1);
	        serverNodeIds.push_back(server_nodeNum1);

            Ptr<Node> server_tmp2 = nodes_net1[2][5].Get (0);
            uint32_t server_nodeNum2 = server_tmp2->GetId();
            serverNodes.Add(server_tmp2);
	        serverNodeIds.push_back(server_nodeNum2);
        }
    else {
    	cout<< "Too many servers, bro!" << endl;
    }
    	

    //clients [1,250] [251,500] [501,750]
    //clientNodes [0,249] [250,499] [500,749]
	
    // 1 campus
    if (clients<=250){
		for (uint32_t i = 0; i < clients; i++){
			Ptr<Node> tmp = clientVector[i];

			uint32_t nodeNum = tmp->GetId();

			sprintf (buffer, "Adding client node: %d", nodeNum);
			NS_LOG_INFO (buffer);

			clientNodes.Add(tmp);
			clientNodeIds.push_back(nodeNum);
			
			int r = rand()%clients; //generate a random number [1,clients]
			
			char   newprefix[64]="/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			//std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			sprintf (newprefix, "%s%d", newprefix,r);
			
			ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			consumerHelper.SetPrefix (newprefix);
			consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			producerHelper.SetPrefix (newprefix);
			producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			producerHelper.Install (nodes_net1[0][5].Get (0));
         }
	}
    // 2 campus
    else if(clients > 250 && clients <= 500) {
            for (uint32_t i = 0; i < 250 ; i++){
			    Ptr<Node> tmp = clientVector[i];

			    uint32_t nodeNum = tmp->GetId();

			    sprintf (buffer, "Adding client node: %d", nodeNum);
			    NS_LOG_INFO (buffer);

			    clientNodes.Add(tmp);
			    clientNodeIds.push_back(nodeNum);
			
			

			    int r = rand()%clients; //generate a random number [1,clients]
			
			    char   newprefix[64]="/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    //std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    sprintf (newprefix, "%s%d", newprefix,r);
			
			
			    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			    consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			    consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			    consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			    consumerHelper.SetPrefix (newprefix);
			    consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			    producerHelper.SetPrefix (newprefix);
			    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			    producerHelper.Install (nodes_net1[0][5].Get (0));
            }

            for (uint32_t i = 250; i < clients ; i++){
			    Ptr<Node> tmp = clientVector[i];

			    uint32_t nodeNum = tmp->GetId();

			    sprintf (buffer, "Adding client node: %d", nodeNum);
			    NS_LOG_INFO (buffer);

			    clientNodes.Add(tmp);
			    clientNodeIds.push_back(nodeNum);
			
			    int r = rand()%clients; //generate a random number [1,clients]
			
			    char   newprefix1[64]="/Dinfo/tokyo/shinjuku/nishiwasedau/net1/server/";
			    //std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    sprintf (newprefix1, "%s%d", newprefix1,r);
				
			    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			    consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			    consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			    consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			    consumerHelper.SetPrefix (newprefix1);
			    consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			    producerHelper.SetPrefix (newprefix1);
			    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			    producerHelper.Install (nodes_net1[1][5].Get (0));
            }
     }
     else if(clients > 500 && clients <= 750){
         for (uint32_t i = 0; i < 250 ; i++){
			    Ptr<Node> tmp = clientVector[i];

			    uint32_t nodeNum = tmp->GetId();

			    sprintf (buffer, "Adding client node: %d", nodeNum);
			    NS_LOG_INFO (buffer);

			    clientNodes.Add(tmp);
			    clientNodeIds.push_back(nodeNum);
			
			

			    int r = rand()%clients; //generate a random number [1,clients]
			
			    char   newprefix0[64]="/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    //std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    sprintf (newprefix0, "%s%d", newprefix0,r);
			
			
			    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			    consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			    consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			    consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			    consumerHelper.SetPrefix (newprefix0);
			    consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			    producerHelper.SetPrefix (newprefix0);
			    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			    producerHelper.Install (nodes_net1[0][5].Get (0));
            }

            for (uint32_t i = 250; i < 500 ; i++){
			    Ptr<Node> tmp = clientVector[i];

			    uint32_t nodeNum = tmp->GetId();

			    sprintf (buffer, "Adding client node: %d", nodeNum);
			    NS_LOG_INFO (buffer);

			    clientNodes.Add(tmp);
			    clientNodeIds.push_back(nodeNum);
			
			

			    int r = rand()%clients; //generate a random number [1,clients]
			
			    char   newprefix1[64]="/Dinfo/tokyo/shinjuku/nishiwasedau/net1/server/";
			    //std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    sprintf (newprefix1, "%s%d", newprefix1,r);
			
			
			    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			    consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			    consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			    consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			    consumerHelper.SetPrefix (newprefix1);
			    consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			    producerHelper.SetPrefix (newprefix1);
			    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			    producerHelper.Install (nodes_net1[1][5].Get (0));
            } 
            
            for (uint32_t i = 500; i < clients ; i++){
			    Ptr<Node> tmp = clientVector[i];

			    uint32_t nodeNum = tmp->GetId();

			    sprintf (buffer, "Adding client node: %d", nodeNum);
			    NS_LOG_INFO (buffer);

			    clientNodes.Add(tmp);
			    clientNodeIds.push_back(nodeNum);
			
			

			    int r = rand()%clients; //generate a random number [1,clients]
			
			    char   newprefix2[64]="/Dinfo/tokyo/shinjuku/toyamawasedau/net1/server/";
			    //std::string prefix = "/Dinfo/tokyo/shinjuku/wasedau/net1/server/";
			    sprintf (newprefix2, "%s%d", newprefix2,r);
			
			
			    ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
			    consumerHelper.SetAttribute ("Frequency", StringValue ("100")); 
			    consumerHelper.SetAttribute ("MaxSeq", IntegerValue (10240));
			    consumerHelper.SetAttribute ("Randomize", StringValue ("exponential"));
			    consumerHelper.SetPrefix (newprefix2);
			    consumerHelper.Install (clientNodes.Get (i));// let every client ask for different content(maybe the same)
				
			    producerHelper.SetPrefix (newprefix2);
			    producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
			    producerHelper.SetAttribute ("Freshness", TimeValue (Seconds(0)));
			    producerHelper.Install (nodes_net1[2][5].Get (0));
            }
     }
     else {
    	 cout << "Too many clients,bro!" << endl;
     }
    	 
    
    // Obtain metrics
	char filename[250];

	// Print server nodes to file
	sprintf(filename, "%s/disaster1-ccn-servers-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);

	NS_LOG_INFO ("Printing node files");
    std::ofstream serverFile;
	serverFile.open (filename);
	for (int i = 0; i < serverNodeIds.size(); i++) {
		serverFile << serverNodeIds[i] << std::endl;
	}
	serverFile.close();
    

	sprintf(filename, "%s/disaster1-ccn-clients-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);

	std::ofstream clientFile;
	clientFile.open (filename);
	for (int i = 0; i < clientNodeIds.size(); i++) {
		clientFile << clientNodeIds[i] << std::endl;
	}
	clientFile.close();

	sprintf (filename, "%s/disaster1-ccn-aggregate-trace-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);
	ndn::L3AggregateTracer::InstallAll(filename, Seconds (1.0));

	sprintf (filename, "%s/disaster1-ccn-rate-trace-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);
	ndn::L3RateTracer::InstallAll (filename, Seconds (1.0));

	sprintf (filename, "%s/disaster1-ccn-app-delays-trace-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);
	ndn::AppDelayTracer::InstallAll (filename);

	sprintf (filename, "%s/disaster1-ccn-drop-trace-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);
	L2RateTracer::InstallAll (filename, Seconds (0.5));

	sprintf (filename, "%s/disaster1-ccn-cs-trace-%02d-%03d-%03d-%0*d.txt", results, networks, servers, clients, 12, contentsize);
	
    ndn::CsTracer::InstallAll (filename, Seconds (0.1));

	//p2p_1gb5ms.EnablePcap ("results/ccn_test0.pcap", nodes_net1[0][5].Get (0)->GetId (), true,true);
    sprintf (filename, "%s/ccn_server-%02d-%03d-%03d-%0*d.pcap", results, networks, servers, clients, 12, contentsize);
    p2p_1gb5ms.EnablePcap (filename, 8, true,true);
	//p2p_1gb5ms.EnablePcap ("results/tcp_test1.pcap", serverNodes.Get(1)->GetId (), true,true);
	
    Simulator::Stop (Seconds (60.0));
	Simulator::Run ();
	Simulator::Destroy ();
	return 0;		
}
