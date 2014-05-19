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
	LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);

	int nCN = 3, nLANClients = 42;
	bool nix = true;
	
	// Char array for output strings
	char buffer[250];

	// These are our scenario arguments
	uint32_t contentsize = 0; // Size in bytes of the content to transfer
	uint32_t clients = 1; // Number of clients in the network
	uint32_t servers = 1; // Number of servers in the network
	uint32_t networks = 1; // Number of additional nodes in the network

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
	tuple<std::vector<Ptr<Node> >, std::vector<Ptr<Node> > > t = assignClientsandServers(clients, servers);

	// Separate the tuple into clients and servers
	std::vector<Ptr<Node> > clientVector = t.get<0> ();
	std::vector<Ptr<Node> > serverVector = t.get<1> ();

	NodeContainer clientNodes;
	std::vector<uint32_t> clientNodeIds;
	NodeContainer serverNodes;
	std::vector<uint32_t> serverNodeIds;
	
	// We have to manually introduce the Ptr<Node> to the NodeContainers
		// We do this to make them easier to control later
		for (uint32_t i = 0; i < clients ; i++)
		{
			Ptr<Node> tmp = clientVector[i];

			uint32_t nodeNum = tmp->GetId();

			sprintf (buffer, "Adding client node: %d", nodeNum);
			NS_LOG_INFO (buffer);

			clientNodes.Add(tmp);
			clientNodeIds.push_back(nodeNum);
		}

		// Do the same for the server NodeContainer
		for (uint32_t i = 0; i < servers; i++)
		{
			Ptr<Node> tmp = serverVector[i];

			uint32_t nodeNum = tmp->GetId();

			sprintf (buffer, "Adding server node: %d", nodeNum);
			NS_LOG_INFO (buffer);

			serverNodes.Add(tmp);
			serverNodeIds.push_back(nodeNum);
		}

	
	/*// Create Traffic Flows
	std::cout << "Creating TCP Traffic Flows:" << std::endl;
	Config::SetDefault ("ns3::OnOffApplication::MaxBytes", UintegerValue (500000));
	Config::SetDefault ("ns3::OnOffApplication::OnTime",
			StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
	Config::SetDefault ("ns3::OnOffApplication::OffTime",
			StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
	Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (512));

	Ptr<UniformRandomVariable> urng = CreateObject<UniformRandomVariable> ();
	int r1;
	double r2;
	for (int z = 0; z < nCN; ++z)
	{
		int x = z + 1;
		if (z == nCN - 1)
		{
			x = 0;
		}
		// Subnet 2 LANs
		std::cout << "  Campus Network " << z << " Flows [ Net2 ";
		for (int i = 0; i < 7; ++i)
		{
			for (int j = 0; j < nLANClients; ++j)
			{
				// Sinks
				PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
						InetSocketAddress (Ipv4Address::GetAny (), 9999));
				ApplicationContainer sinkApp = sinkHelper.Install (
						nodes_net2LAN[z][i][j].Get (0));
				sinkApp.Start (Seconds (0.0));
				// Sources
				r1 = 2 + (int)(4 * urng->GetValue ());
				r2 = 10 * urng->GetValue ();
				OnOffHelper client ("ns3::TcpSocketFactory", Address ());
				AddressValue remoteAddress (InetSocketAddress (
						ifs2LAN[z][i][j].GetAddress (0), 9999));
				client.SetAttribute ("Remote", remoteAddress);
				ApplicationContainer clientApp;
				clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
				clientApp.Start (Seconds (r2));
			}
		}
		// Subnet 3 LANs
		std::cout << "Net3 ]" << std::endl;
		for (int i = 0; i < 5; ++i)
		{
			for (int j = 0; j < nLANClients; ++j)
			{
				// Sinks
				PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
						InetSocketAddress (Ipv4Address::GetAny (), 9999));
				ApplicationContainer sinkApp = sinkHelper.Install (
						nodes_net3LAN[z][i][j].Get (0));
				sinkApp.Start (Seconds (0.0));
				// Sources
				r1 = 2 + (int)(4 * urng->GetValue ());
				r2 = 10 * urng->GetValue ();
				OnOffHelper client ("ns3::TcpSocketFactory", Address ());
				AddressValue remoteAddress (InetSocketAddress (
						ifs3LAN[z][i][j].GetAddress (0), 9999));
				client.SetAttribute ("Remote", remoteAddress);
				ApplicationContainer clientApp;
				clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
				clientApp.Start (Seconds (r2));
			}
		}
	}

	std::cout << "Created " << NodeList::GetNNodes () << " nodes." << std::endl;
	TIMER_TYPE routingStart;
	TIMER_NOW (routingStart);

	if (nix)
	{
		// Calculate routing tables
		std::cout << "Using Nix-vectors..." << std::endl;
	}
	else
	{
		// Calculate routing tables
		std::cout << "Populating Global Static Routing Tables..." << std::endl;
		Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	}

	TIMER_TYPE routingEnd;
	TIMER_NOW (routingEnd);
	std::cout << "Routing tables population took "
			<< TIMER_DIFF (routingEnd, routingStart) << std::endl;

	Simulator::ScheduleNow (Progress);
	std::cout << "Running simulator..." << std::endl;
	TIMER_NOW (t1);
	Simulator::Stop (Seconds (100.0));
	Simulator::Run ();
	TIMER_NOW (t2);
	std::cout << "Simulator finished." << std::endl;
	Simulator::Destroy ();

	double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
	std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
	std::cout << "Simulator init time: " << d1 << std::endl;
	std::cout << "Simulator run time: " << d2 << std::endl;
	std::cout << "Total elapsed time: " << d1+d2 << std::endl;

	delete[] nodes_netLR;*/

	// Calculate routing tables
	
		//std::cout << "Populating Global Static Routing Tables..." << std::endl;
	//Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	ndn::StackHelper ndnHelper;
	//ndnHelper.SetDefaultRoutes (true);
	ndnHelper.InstallAll ();

	ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
		ndnGlobalRoutingHelper.InstallAll ();
		ndnGlobalRoutingHelper.AddOrigins ("/Dinfo/tokyo/shinjuku/waseda-u/waseda", serverNodes);
		ndn::GlobalRoutingHelper::CalculateRoutes ();

	// Consumer
	ndn::AppHelper consumerHelper ("ns3::ndn::ConsumerCbr");
	// Consumer will request /prefix/0, /prefix/1, ...
	consumerHelper.SetPrefix ("/Dinfo/tokyo/shinjuku/waseda-u/waseda");
	consumerHelper.SetAttribute ("Frequency", StringValue ("100")); // 10 interests a second
	//consumerHelper.Install (nodes.Get (12)); // first node
	consumerHelper.Install (clientNodes);


	// Producer
	ndn::AppHelper producerHelper ("ns3::ndn::Producer");
	// Producer will reply to all requests starting with /prefix
	producerHelper.SetPrefix ("/Dinfo/tokyo/shinjuku/waseda-u/waseda");
	producerHelper.SetAttribute ("PayloadSize", StringValue("1024"));
	//producerHelper.Install (nodes.Get (2)); // last node
	producerHelper.Install (serverNodes);

	// Obtain metrics
	ndn::L3AggregateTracer::InstallAll("results/disaster-ccn-aggregate-trace.txt", Seconds (1.0));
	ndn::L3RateTracer::InstallAll ("results/disaster-ccn-rate-trace.txt", Seconds (1.0));
	ndn::AppDelayTracer::InstallAll ("results/disaster-ccn-app-delays-trace.txt");
	L2RateTracer::InstallAll ("results/disaster-ccn-drop-trace.txt", Seconds (0.5));

	Simulator::Stop (Seconds (20.0));

	Simulator::Run ();
	Simulator::Destroy ();
	return 0;
}
