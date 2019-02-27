/*
 * Copyright (c) 2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Ron Dreslinski
 */

/**
 * @file
 * Describes a tagged prefetcher based on template policies.
 */

#include "mem/cache/prefetch/markov.hh"
#include "debug/Markov.hh"

MarkovPrefetcher::MarkovPrefetcher(const MarkovPrefetcherParams *p)
    : QueuedPrefetcher(p), 
      degree(p->degree),
      num_entries(p->num_entries)
{
    markovTable.resize(num_entries);
    for (size_t i =0; i<num_entries; i++){
        markovTable[i].currentAddress = 0;
        markovTable[i].nextAddresses.resize(degree);
    }

    previousMiss = 0;
}

void
MarkovPrefetcher::calculatePrefetch(const PacketPtr &pkt,
        std::vector<AddrPriority> &addresses)
{
    if(pkt->getAddr() == previousMiss)
        return;
        
    Addr blkAddr = pkt->getAddr() & ~(Addr)(blkSize-1);
    std::list<Addr>::iterator it;
    size_t index;
    DPRINTF(Markov, "Block size %d \n", blkSize);
    DPRINTF(Markov, "Miss address is: %x , full addr %x \n", blkAddr, pkt->getAddr());
    /** Update previousMiss's nextAddresses **/
    if(previousMiss != 0){
        index = (previousMiss >> lBlkSize) 
                & (Addr)(num_entries-1);
        
        //check if already exists in nextAddresses
        int flag = 0;
        for (it = markovTable[index].nextAddresses.begin(); 
        it != markovTable[index].nextAddresses.end(); ++it) {
            if(*it == blkAddr){
                flag = 1;
                break;
            }
        }

        if (flag){
            //already exists
            //move to the front
            markovTable[index].nextAddresses.splice(
                markovTable[index].nextAddresses.begin(),
                markovTable[index].nextAddresses,
                it);
        }else{
            //pop back and push front
            markovTable[index].nextAddresses.pop_back();
            markovTable[index].nextAddresses.push_front(blkAddr);
        }
    }

    /** Update entry for currentAddress **/
    previousMiss = pkt->getAddr();
    index = (previousMiss >> lBlkSize) 
            & (Addr)(num_entries-1);
    
    // if (markovTable[index].currentAddress==blkAddr){
        //hit
        it = markovTable[index].nextAddresses.begin();
        for (int d = 0; d < degree; d++) {

            Addr newAddr = *it;
            if (newAddr!=0){
                if (!samePage(blkAddr, newAddr)) {
                    // Count number of unissued prefetches due to page crossing
                    pfSpanPage += 1;
                } else {
                    addresses.push_back(AddrPriority(newAddr,degree-1-d));
                    DPRINTF(Markov, "Issued address is: %x, priority %d \n", newAddr, degree-1-d);
                }
            }
            
            ++it;
        }
    // }else {
    //     //either conflit or miss
    //     //just overwrite
    //     markovTable[index].currentAddress=blkAddr;
    //     markovTable[index].nextAddresses.resize(degree, 0);
    // }

}

MarkovPrefetcher*
MarkovPrefetcherParams::create()
{
   return new MarkovPrefetcher(this);
}
