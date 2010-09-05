// -----------------------------------------------------------------------
//   
//   Copyright 2004 Tommy Thorn - All Rights Reserved
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
//   Bostom MA 02111-1307, USA; either version 2 of the License, or
//   (at your option) any later version; incorporated herein by reference.
//
// -----------------------------------------------------------------------

// Dummy simulations of Altera PLLs

`timescale 1ns/10ps
module pll1(input wire inclk0,
            output wire c0,
            output wire c1,
            output wire locked,
            output wire e0);
   
   assign          c0 = inclk0;
   assign          c1 = inclk0;
   assign          locked = 1;
   assign          e0 = inclk0;
endmodule

module pll2(input wire inclk0,
            output wire c0,
            output wire c1,
            output wire locked,
            output wire e0);
   
   assign          c0 = inclk0;
   assign          c1 = inclk0;
   assign          locked = 1;
   assign          e0 = inclk0;
endmodule
