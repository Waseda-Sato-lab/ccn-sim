//Interest Processing
function Process(Interest):
	Name ← Interest.Name
	
	if Data ← ContentStore.Find(Name) then
		Return(Data)
	else 
		if PitEntry ← PIT.Find(Name) then
			if Interest.Nonce ∈ PitEntry.NonceList then
				Return Interest NACK (Duplicate)
				Stop processing
			end if
			if PitEntry.RetryTimer is expired then
				Forward(Interest, PitEntry)
				Stop processing
			end if
			Add Interest.Interface to PitEntry.Incoming
		else 
			PitEntry ← PIT.Create(Interest)
			PitEntry.Incoming ← Interest.Interface
			Forward(Interest, PitEntry)
		end if	
	end if
end function
	
//Forwarding Strategy	
function Forward(Interest, PitEntry):
	if FibEntry ← FIB.Find(Interest.Name) then
		for each interface in FibEntry by rank do
			if interface doesn't belong to PitEntry.Outgoing then	
				if interface.Avaialble then
					Set PitEntry.RetryTimer
					Transmit(interface, Interest)
					Add interface to PitEntry.Outgoing
					if ProbingDue(FibEntry) then
						Probe(Interest, PitEntry)
					end if
					Stop processing
				end if
			end if		
		end for
		Return Interest NACK (Congestion)
		GiveUp(Interest)
	else
		Return Interest NACK (No Data)
		GiveUp(Interest)
	end if
end function

//Interest NACK Processing (Retry)	
function Process(NACK)
	PitEntry ← PIT.Find (NACK.Name)
	if PitEntry ≡ null or 
	   PitEntry.RetryTimer expired or 
	   NACK.Nonce doesn't belong to PitEntry.NonceList then
	Stop processing
	end if
	Forward (NACK.Interest, PitEntry)
end function