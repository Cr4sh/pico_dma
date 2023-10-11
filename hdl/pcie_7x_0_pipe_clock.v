//
// PIPE Clock Module for 7 Series Transceiver
//

`timescale 1ns / 1ps

module pcie_7x_0_pipe_clock #
(
  parameter PCIE_ASYNC_EN = "FALSE",        // PCIe async enable
  parameter PCIE_TXBUF_EN = "FALSE",        // PCIe TX buffer enable for Gen1/Gen2 only
  parameter PCIE_CLK_SHARING_EN = "FALSE",  // Enable Clock Sharing
  parameter PCIE_LANE = 1,                  // PCIe number of lanes
  parameter PCIE_LINK_SPEED = 3,            // PCIe link speed 
  parameter PCIE_REFCLK_FREQ = 0,           // PCIe reference clock frequency
  parameter PCIE_USERCLK1_FREQ = 2,         // PCIe user clock 1 frequency
  parameter PCIE_USERCLK2_FREQ = 2,         // PCIe user clock 2 frequency
  parameter PCIE_OOBCLK_MODE = 1,           // PCIe oob clock mode
  parameter PCIE_DEBUG_MODE = 0             // PCIe Debug mode    
)(
  //
  // Input
  //
  input CLK_CLK,
  input CLK_TXOUTCLK,
  input [PCIE_LANE - 1 : 0] CLK_RXOUTCLK_IN,
  input CLK_RST_N,
  input [PCIE_LANE - 1 : 0] CLK_PCLK_SEL,
  input [PCIE_LANE - 1 : 0] CLK_PCLK_SEL_SLAVE,
  input CLK_GEN3,
  
  //  
  // Output
  //
  output CLK_PCLK,
  output CLK_PCLK_SLAVE,
  output CLK_RXUSRCLK,
  output [PCIE_LANE - 1 : 0] CLK_RXOUTCLK_OUT,
  output CLK_DCLK,
  output CLK_OOBCLK,
  output CLK_USERCLK1,
  output CLK_USERCLK2,
  output CLK_MMCM_LOCK  
);    
  
  // 
  // Select clock divider
  //
  localparam DIVCLK_DIVIDE = (PCIE_REFCLK_FREQ == 2) ? 1 :
                             (PCIE_REFCLK_FREQ == 1) ? 1 : 1;
                                               
  localparam CLKFBOUT_MULT_F = (PCIE_REFCLK_FREQ == 2) ? 4 :
                               (PCIE_REFCLK_FREQ == 1) ? 8 : 10;
               
  localparam CLKIN1_PERIOD = (PCIE_REFCLK_FREQ == 2) ? 4 :
                             (PCIE_REFCLK_FREQ == 1) ? 8 : 10;
                                               
  localparam CLKOUT0_DIVIDE_F = 8;
  localparam CLKOUT1_DIVIDE = 4;
    
  localparam CLKOUT2_DIVIDE = (PCIE_USERCLK1_FREQ == 5) ?  2 : 
                              (PCIE_USERCLK1_FREQ == 4) ?  4 :
                              (PCIE_USERCLK1_FREQ == 3) ?  8 :
                              (PCIE_USERCLK1_FREQ == 1) ? 32 : 16;
                                               
  localparam CLKOUT3_DIVIDE = (PCIE_USERCLK2_FREQ == 5) ?  2 : 
                              (PCIE_USERCLK2_FREQ == 4) ?  4 :
                              (PCIE_USERCLK2_FREQ == 3) ?  8 :
                              (PCIE_USERCLK2_FREQ == 1) ? 32 : 16;
                                           
  localparam CLKOUT4_DIVIDE = 20;
  localparam PCIE_GEN1_MODE = 1'b1;                                   
  
  //     
  // Input registers
  //
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg [PCIE_LANE - 1 : 0] pclk_sel_reg1 = { PCIE_LANE{1'd0} };
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg [PCIE_LANE - 1 : 0] pclk_sel_reg2 = { PCIE_LANE{1'd0} };
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg [PCIE_LANE - 1 : 0] pclk_sel_slave_reg1 = { PCIE_LANE{1'd0} };        
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg [PCIE_LANE - 1 : 0] pclk_sel_slave_reg2 = { PCIE_LANE{1'd0} };
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg gen3_reg1 = 1'd0;
  (* ASYNC_REG = "TRUE", SHIFT_EXTRACT = "NO" *) reg gen3_reg2 = 1'd0;   
     
  //     
  // Internal signals
  // 
  wire refclk;
  wire mmcm_fb;
  wire clk_125mhz;
  wire clk_125mhz_buf;
  wire clk_250mhz;
  wire userclk1;
  wire userclk2;
  wire oobclk;  
  reg pclk_sel_slave = 1'd0;
  (* dont_touch = "true" *) reg pclk_sel = 1'd0;

  //
  // Output registers
  //
  wire pclk_1;
  wire pclk;
  wire userclk1_1;
  wire userclk2_1;
  wire mmcm_lock;
    
  //  
  // Generate per-lane signals
  //
  genvar i; // index for per-lane signals

  //
  // Input FF
  //
  always @ (posedge pclk) begin

    if (!CLK_RST_N) begin
    
      // 1-st stage FF
      pclk_sel_reg1 <= { PCIE_LANE{1'd0} };
      pclk_sel_slave_reg1 <= { PCIE_LANE{1'd0} };
      gen3_reg1 <= 1'd0;
      
      // 2-nd stage FF
      pclk_sel_reg2 <= { PCIE_LANE{1'd0} };
      pclk_sel_slave_reg2 <= { PCIE_LANE{1'd0} };
      gen3_reg2 <= 1'd0;
    
    end else begin
      
      // 1-st stage FF
      pclk_sel_reg1 <= CLK_PCLK_SEL;
      pclk_sel_slave_reg1 <= CLK_PCLK_SEL_SLAVE;
      gen3_reg1 <= CLK_GEN3;
      
      // 2-nd stage FF
      pclk_sel_reg2 <= pclk_sel_reg1;
      pclk_sel_slave_reg2 <= pclk_sel_slave_reg1;
      gen3_reg2 <= gen3_reg1;
      
    end  
  end

  // 
  // Select reference clock or TXOUTCLK
  //   
  generate if ((PCIE_TXBUF_EN == "TRUE") && (PCIE_LINK_SPEED != 3))

    begin : refclk_i

      // select reference clock 
      BUFG refclk_i(.I( CLK_CLK ), .O( refclk ));
      
    end else begin : txoutclk_i
    
      // select TXOUTCLK
      BUFG txoutclk_i(.I( CLK_TXOUTCLK ), .O( refclk ));
   
    end
  endgenerate

  //
  // MMCM
  //
  MMCME2_ADV #
  (
    .BANDWIDTH( "OPTIMIZED" ),
    .CLKOUT4_CASCADE( "FALSE" ),
    .COMPENSATION( "ZHOLD" ),
    .STARTUP_WAIT( "FALSE" ),
    .DIVCLK_DIVIDE( DIVCLK_DIVIDE ),
    .CLKFBOUT_MULT_F( CLKFBOUT_MULT_F ),  
    .CLKFBOUT_PHASE( 0.000 ),
    .CLKFBOUT_USE_FINE_PS( "FALSE" ),
    .CLKOUT0_DIVIDE_F( CLKOUT0_DIVIDE_F ),                    
    .CLKOUT0_PHASE( 0.000 ),
    .CLKOUT0_DUTY_CYCLE( 0.500 ),
    .CLKOUT0_USE_FINE_PS( "FALSE" ),
    .CLKOUT1_DIVIDE( CLKOUT1_DIVIDE ),                    
    .CLKOUT1_PHASE( 0.000 ),
    .CLKOUT1_DUTY_CYCLE( 0.500 ),
    .CLKOUT1_USE_FINE_PS( "FALSE" ),
    .CLKOUT2_DIVIDE( CLKOUT2_DIVIDE ),                  
    .CLKOUT2_PHASE( 0.000 ),
    .CLKOUT2_DUTY_CYCLE( 0.500 ),
    .CLKOUT2_USE_FINE_PS( "FALSE" ),
    .CLKOUT3_DIVIDE( CLKOUT3_DIVIDE ),                  
    .CLKOUT3_PHASE( 0.000 ),
    .CLKOUT3_DUTY_CYCLE( 0.500 ),
    .CLKOUT3_USE_FINE_PS( "FALSE" ),
    .CLKOUT4_DIVIDE( CLKOUT4_DIVIDE ),                  
    .CLKOUT4_PHASE( 0.000 ),
    .CLKOUT4_DUTY_CYCLE( 0.500 ),
    .CLKOUT4_USE_FINE_PS( "FALSE" ),
    .CLKIN1_PERIOD( CLKIN1_PERIOD ),                   
    .REF_JITTER1( 0.010 )    
  )
  mmcm_i
  (
     // Input
    .CLKIN1( refclk ),
    .CLKIN2( 1'd0 ),
    .CLKINSEL( 1'd1 ),
    .CLKFBIN( mmcm_fb ),
    .RST( !CLK_RST_N ),
    .PWRDWN( 1'd0 ), 
    
    // Output
    .CLKFBOUT( mmcm_fb ),
    .CLKOUT0( clk_125mhz ),
    .CLKOUT1( clk_250mhz ),
    .CLKOUT2( userclk1 ),
    .CLKOUT3( userclk2 ),
    .CLKOUT4( oobclk ),
    .LOCKED( mmcm_lock ),
    
    // Dynamic reconfiguration
    .DCLK( 1'd0 ),
    .DADDR( 7'd0 ),
    .DEN( 1'd0 ),
    .DWE( 1'd0 ),
    .DI( 16'd0 ),
    
    // Dynamic phase shift
    .PSCLK( 1'd0 ),
    .PSEN( 1'd0 ),
    .PSINCDEC( 1'd0 )
  ); 
  
  //
  // Select PCLK mux or PCLK buffer
  //
  generate if (PCIE_LINK_SPEED != 1) 

    begin : pclk_i1_bufgctrl
    
      // PCLK mux
      BUFGCTRL pclk_i1
      (
        .CE0( 1'd1 ),         
        .CE1( 1'd1 ),        
        .I0( clk_125mhz ),   
        .I1( clk_250mhz ),   
        .IGNORE0( 1'd0 ),        
        .IGNORE1( 1'd0 ),        
        .S0( ~pclk_sel ),    
        .S1( pclk_sel ),    
        .O( pclk_1 )
      );
    
    end else begin : pclk_i1_bufg 
      
      // PCLK buffer
      BUFG pclk_i1(.I( clk_125mhz ), .O( clk_125mhz_buf ));
      
      assign pclk_1 = clk_125mhz_buf;
      
    end 
  endgenerate

  //
  // Select PCLK mux for slave or PCLK buffer
  //
  generate if (PCIE_CLK_SHARING_EN == "FALSE")
 
    begin : pclk_slave_disable
    
      // PCLK mux
      assign CLK_PCLK_SLAVE = 1'b0;
    
    end else if (PCIE_LINK_SPEED != 1) begin : pclk_slave_bufgctrl
    
      // PCLK mux
      BUFGCTRL pclk_slave
      (
        .CE0( 1'd1 ),         
        .CE1( 1'd1 ),        
        .I0( clk_125mhz ),   
        .I1( clk_250mhz ),   
        .IGNORE0( 1'd0 ),        
        .IGNORE1( 1'd0 ),        
        .S0( ~pclk_sel_slave ),    
        .S1( pclk_sel_slave ),    
        .O( CLK_PCLK_SLAVE )
      );
    
    end else begin : pclk_slave_bufg 
    
      // PCLK buffer
      BUFG pclk_slave(.I( clk_125mhz ), .O( CLK_PCLK_SLAVE ));
    
    end 
  endgenerate

  //
  // Generate RXOUTCLK buffer for debug
  //
  generate if ((PCIE_DEBUG_MODE == 1) || (PCIE_ASYNC_EN == "TRUE"))
        
    begin : rxoutclk_per_lane
    
      // generate per Lane
      for ( i = 0; i < PCIE_LANE; i = i + 1) 
    
        begin : rxoutclk_i
        
          // RXOUTCLK buffer
          BUFG rxoutclk_i(.I( CLK_RXOUTCLK_IN[i] ), .O( CLK_RXOUTCLK_OUT[i] ));
        
        end
      end         
    else
     
    begin : rxoutclk_i_disable
    
      // disable RXOUTCLK buffer for normal operation
      assign CLK_RXOUTCLK_OUT = { PCIE_LANE{1'd0} };
    
    end                   
  endgenerate 

  //
  // Generate DCLK buffer
  //
  generate if (PCIE_LINK_SPEED != 1)

    begin : dclk_i_bufg
    
      // DCLK buffer
      BUFG dclk_i(.I( clk_125mhz ), .O( CLK_DCLK ));
      
    end else begin : dclk_i
    
      // always 125 MHz in Gen1
      assign CLK_DCLK = clk_125mhz_buf; 
      
    end
  endgenerate

  //
  // Generate USERCLK1 buffer
  //
  generate if (PCIE_GEN1_MODE == 1'b1 && PCIE_USERCLK1_FREQ == 3)
       
    begin : userclk1_i1_no_bufg
    
      // USERCLK1 same as PCLK
      assign userclk1_1 = pclk_1;
      
    end else begin : userclk1_i1
    
      // USERCLK1 buffer
      BUFG usrclk1_i1(.I( userclk1 ), .O( userclk1_1 ));
 
    end 
  endgenerate 

  //
  // Generate USERCLK2 buffer
  //
  generate if (PCIE_GEN1_MODE == 1'b1 && PCIE_USERCLK2_FREQ == 3 )  

    begin : userclk2_i1_no_bufg0
    
      // USERCLK2 same as PCLK
      assign userclk2_1 = pclk_1;
      
    end else if (PCIE_USERCLK2_FREQ == PCIE_USERCLK1_FREQ ) begin : userclk2_i1_no_bufg1
    
      // USERCLK2 same as USERCLK1
      assign userclk2_1 = userclk1_1;
      
    end else begin : userclk2_i1
    
      // USERCLK2 buffer
      BUFG usrclk2_i1(.I( userclk2 ), .O( userclk2_1 ));
      
    end
  endgenerate 

  //
  // Generate OOBCLK buffer
  //
  generate if (PCIE_OOBCLK_MODE == 2) 

    begin : oobclk_i1
    
      // OOBCLK buffer
      BUFG oobclk_i1(.I( oobclk ), .O( CLK_OOBCLK ));
      
    end else begin : oobclk_i1_disable
    
      // disable OOBCLK buffer
      assign CLK_OOBCLK = pclk;
      
    end  
  endgenerate 

  //
  // Disabled second stage buffers
  //
  assign pclk = pclk_1;
  assign CLK_RXUSRCLK = pclk_1;
  assign CLK_USERCLK1 = userclk1_1;
  assign CLK_USERCLK2 = userclk2_1;
 
  //
  // Select PCLK
  //
  always @ (posedge pclk) begin

    if (!CLK_RST_N)
    
      pclk_sel <= 1'd0;
        
    else  begin
           
      if (&pclk_sel_reg2)

        // select 250 MHz      
        pclk_sel <= 1'd1;
                 
      else if (&(~pclk_sel_reg2))
         
        // select 125 MHz
        pclk_sel <= 1'd0;  

      else
             
        // hold PCLK 
        pclk_sel <= pclk_sel;
        
    end
  end        

  always @ (posedge pclk) begin

    if (!CLK_RST_N)
    
      pclk_sel_slave <= 1'd0;
        
    else begin     
        
      if (&pclk_sel_slave_reg2)
        
        // select 250 MHz            
        pclk_sel_slave <= 1'd1;
                      
      else if (&(~pclk_sel_slave_reg2))
        
        // select 125 MHz
        pclk_sel_slave <= 1'd0;  
        
      else
        
        // hold PCLK
        pclk_sel_slave <= pclk_sel_slave;
            
    end
  end     

  //
  // PIPE clock output
  //
  assign CLK_PCLK = pclk;
  assign CLK_MMCM_LOCK = mmcm_lock;

endmodule
