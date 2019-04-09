// Copyright(c) 2018 Costantino Grana, Federico Bolelli 
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met :
//
// *Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and / or other materials provided with the distribution.
//
// * Neither the name of GRAPHSGEN nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "ruleset_generator.h"

#include <string>

using namespace std;

// First subiteration
rule_set generate_thin_gh()
{
    pixel_set gh_mask {
        { "P9", {-1, -1} }, { "P2", {0, -1} }, { "P3", {+1, -1} },
        { "P8", {-1,  0} }, { "P1", {0,  0} }, { "P4", {+1,  0} },
        { "P7", {-1, +1} }, { "P6", {0, +1} }, { "P5", {+1, +1} },
    };

    rule_set thinning;
    thinning.InitConditions(gh_mask);
    thinning.AddCondition("iter");
    thinning.InitActions({
        "keep0",
        "keep1",
        "change0",
        });


    thinning.generate_rules([](rule_set& rs, uint i) {
        rule_wrapper r(rs, i);

        int P1 = r["P1"];
        int P2 = r["P2"];
        int P3 = r["P3"];
        int P4 = r["P4"];
        int P5 = r["P5"];
        int P6 = r["P6"];
        int P7 = r["P7"];
        int P8 = r["P8"];
        int P9 = r["P9"];
        if (!P1) {
            r << "keep0";
            return;
        }

        int C = ((!P2) & (P3 | P4)) + ((!P4) & (P5 | P6)) +
                ((!P6) & (P7 | P8)) + ((!P8) & (P9 | P2));
        int N1 = (P9 | P2) + (P3 | P4) + (P5 | P6) + (P7 | P8);
        int N2 = (P2 | P3) + (P4 | P5) + (P6 | P7) + (P8 | P9);
        int N = N1 < N2 ? N1 : N2;
        
        int m;
        if (r["iter"] == 0) {
            m = (P6 | P7 | (!P9)) & P8;
        }
        else {
            m = (P2 | P3 | (!P5)) & P4;
        }
        
        if (
            /*(a)*/ (C == 1) && 
            /*(b)*/ (2 <= N && N <= 3) &&
            /*(c)*/ m == 0 
            )
            r << "change0";
        else
            r << "keep1";
    });

    return thinning;
}
