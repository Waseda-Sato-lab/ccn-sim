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

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"

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
  MobilityHelper mobility;
  // Put everybody into a line
  Ptr<ListPositionAllocator> initialAlloc = 
    CreateObject<ListPositionAllocator> ();
      initialAlloc->Add (Vector (16.0, 22, 0.));
      initialAlloc->Add (Vector (45.0, 37, 0.));
      initialAlloc->Add (Vector (53.0, 8, 0.));

  mobility.SetPositionAllocator(initialAlloc);

 mobility.SetMobilityModel("ns3::WaypointMobilityModel");
 mobility.Install(train.Get(0));
  // Set mobility random number streams to fixed values
  //mobility.AssignStreams (sta, 0);
 Ptr<WaypointMobilityModel> staWaypointMobility = DynamicCast<WaypointMobilityModel>( train.Get(0)->GetObject<MobilityModel>());
  // AsciiTraceHelper ascii;
  //MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("mobility-trace-example.mob"));
 staWaypointMobility->AddWaypoint(Waypoint(Seconds(1.0),Vector(10.0,9,0)));
 staWaypointMobility->AddWaypoint(Waypoint(Seconds(4.0), Vector(16.0,22,0)));
staWaypointMobility->AddWaypoint(Waypoint(Seconds(8.0), Vector(16.0,22,0)));
 staWaypointMobility->AddWaypoint (Waypoint (Seconds (13.0),Vector (45.0, 37, 0)));
 staWaypointMobility->AddWaypoint(Waypoint (Seconds (15.0),Vector (53.0, 8,0)));
  Simulator::Stop (Seconds (40.0));
  Simulator::Run ();
  Simulator::Destroy ();
}
