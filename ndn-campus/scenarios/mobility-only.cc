/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 University of Washington
 *
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
 */

// Test that mobility tracing works

#include <iostream>

#include <ns3-dev/ns3/core-module.h>
#include <ns3-dev/ns3/network-module.h>
#include <ns3-dev/ns3/mobility-module.h>

using namespace ns3;

int main (int argc, char *argv[])
{
	CommandLine cmd;
	cmd.Parse (argc, argv);

	// One train & three stations
	NodeContainer train;
	train.Create(1);
	NodeContainer sta;
	sta.Create (5);

	// How to get/set sta's location and presentate them into the form of Vector(a,b,c)?
	// Ptr<ConstantPositionMobilityModel> Loc =  n->GetObject<ConstantPositionMobilityModel> ();
	// if (Loc == 0)
	//     {
	//    Loc = CreateObject<ConstantPositionMobilityModel> ();
	//    n->AggregateObject (Loc);
	//     }
	// Vector vec (10, 20, 0);
	// Loc->SetPosition (vec);
	MobilityHelper mobilityStations;

	mobilityStations.SetPositionAllocator ("ns3::GridPositionAllocator",
			"MinX", DoubleValue (0.0),
			"MinY", DoubleValue (0.0),
			"DeltaX", DoubleValue (20.0),
			"DeltaY", DoubleValue (0.0),
			"GridWidth", UintegerValue (sta.GetN()),
			"LayoutType", StringValue ("RowFirst"));
	mobilityStations.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

	mobilityStations.Install (sta);

	MobilityHelper mobilityTrain;

	Vector diff = Vector(0.0, 8.0, 0.0);

	Vector pos;

	Ptr<ListPositionAllocator> initialAlloc = CreateObject<ListPositionAllocator> ();

	Ptr<MobilityModel> mob = sta.Get(0)->GetObject<MobilityModel>();

	pos = mob->GetPosition();

	initialAlloc->Add (pos + diff);

	// Put everybody into a line
	//initialAlloc->Add (Vector (45.0, 37, 0.));
	//initialAlloc->Add (Vector (53.0, 8, 0.));
	mobilityTrain.SetPositionAllocator(initialAlloc);
	mobilityTrain.SetMobilityModel("ns3::WaypointMobilityModel");
	mobilityTrain.Install(train.Get(0));

	// Set mobility random number streams to fixed values
	//mobility.AssignStreams (sta, 0);

	Ptr<WaypointMobilityModel> staWaypointMobility = DynamicCast<WaypointMobilityModel>( train.Get(0)->GetObject<MobilityModel>());

	double sec = 0.0;
	double waitint = 0.3;
	double travelTime = 2.0;

	for (int j; j < sta.GetN(); j++)
	{
		mob = sta.Get(j)->GetObject<MobilityModel>();

		Vector wayP = mob->GetPosition() + diff;

		staWaypointMobility->AddWaypoint(Waypoint(Seconds(sec), wayP));
		staWaypointMobility->AddWaypoint(Waypoint(Seconds(sec + waitint), wayP));

		sec += waitint + travelTime;
	}

	Simulator::Stop (Seconds (40.0));
	Simulator::Run ();
	Simulator::Destroy ();
}
