`define AWM1 31
`define REQ     [`AWM1+38:0]
`define RES           [32:0]

// Request subfields
`define A      [`AWM1+38:38]
`define R               [37]
`define W               [36]
`define WD           [35: 4]
`define WBE          [ 3: 0]

// Result subfields
`define HOLD             [0]
`define WAIT             [0]  // synonym
`define RD            [32:1]

