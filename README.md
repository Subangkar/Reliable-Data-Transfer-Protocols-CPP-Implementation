Reliable Data Transfer Protocol implementation in C++ using modified ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR
FROM VERSION 1.1 of J.F.Kurose as an assignment in Computer Networking Lab in Level-3, Term-2 of BUET.    
  
In this assignment sender and receiver implemented for two versions of RDT3.0 are implemented for only simplex data transfer:  
 - Alternating-bit protocol
 - Go-Back-N with finite sequence numbers [0,7].  
  
  
Additional Implementation:
 - separate timers used for each packet in sender's window.  
 - another modified version of go-back-n is implemented which sends cumulative ack after certain timeout unlike normal gbn.  
 
 
 
Detailed specifications can be found at "offline_specification.pdf"  in folder "Assignment-Specifications".  
  
Simulation:
 - compile and run `rdt_abp.cpp` to simulate Alternating-bit protocol.  
 - compile and run `rdt_gbn.cpp` to simulate Go-Back-N protocol.  
 - compile and run `rdt_gbn_cumulative_ack.cpp` to simulate Go-Back-N protocol with modified receiver.  
 
This repo has been worked in CLion project and that's why `CMakeLists.txt` exists here.  