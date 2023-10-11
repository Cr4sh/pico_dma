//
// Top level module
//

`timescale 1 ps / 1 ps

`define PCI_EXP_EP_OUI 24'h000A35

//
// Device Serial Number (DSN) constants
//
`define PCI_EXP_EP_DSN_2 32'h00000001
`define PCI_EXP_EP_DSN_1 {{ 8'h1 }, `PCI_EXP_EP_OUI }

module pcie_microblaze_top #
(
  parameter PL_FAST_TRAIN = "FALSE",        // Simulation Speedup
  parameter EXT_PIPE_SIM = "FALSE",         // This Parameter has effect on selecting Enable External PIPE Interface in GUI.	
  parameter PCIE_EXT_CLK = "TRUE",          // Use External Clocking Module
  parameter REF_CLK_FREQ = 0,               // 0 - 100 MHz, 1 - 125 MHz, 2 - 250 MHz
  parameter C_DATA_WIDTH = 64,              // RX/TX interface data width
  parameter KEEP_WIDTH = C_DATA_WIDTH / 8   // TSTRB width
)(
  input sys_clk_n,
  input sys_clk_p,
  input sys_rst_n,
  input [1:0] user_io,
  output [2:0] user_led,
  input uart_rxd,
  output uart_txd,
  output clkreq_l,
  output pcie_txp,
  output pcie_txn,
  input pcie_rxp,
  input pcie_rxn,
  inout spi_io0_io,
  inout spi_io1_io,
  inout spi_io2_io,
  inout spi_io3_io,
  inout spi_ss_io
);

  assign clkreq_l = 1'b0;
  
  //
  // Clock and reset
  //
  wire pipe_mmcm_rst_n;
  wire user_clk;
  wire user_reset;
  wire user_lnk_up;

  //
  // Transmit
  //
  wire s_axis_tx_tready;
  wire [3:0] s_axis_tx_tuser;
  wire [C_DATA_WIDTH - 1 : 0] s_axis_tx_tdata;
  wire [KEEP_WIDTH - 1 : 0] s_axis_tx_tkeep;
  wire s_axis_tx_tlast;
  wire s_axis_tx_tvalid;

  //
  // Receive
  //
  wire [C_DATA_WIDTH - 1 : 0] m_axis_rx_tdata;
  wire [KEEP_WIDTH - 1 : 0] m_axis_rx_tkeep;
  wire m_axis_rx_tlast;
  wire m_axis_rx_tvalid;
  wire m_axis_rx_tready;
  wire [21:0] m_axis_rx_tuser;

  //
  // Common
  //
  wire tx_cfg_gnt;
  wire rx_np_ok;
  wire rx_np_req;
  wire cfg_turnoff_ok;
  wire cfg_trn_pending;
  wire cfg_pm_halt_aspm_l0s;
  wire cfg_pm_halt_aspm_l1;
  wire cfg_pm_force_state_en;
  wire [1:0] cfg_pm_force_state;
  wire cfg_pm_wake;
  wire [63:0] cfg_dsn;

  //
  // Flow vontrol
  //
  wire [2:0] fc_sel;

  //
  // Configuration interface
  //
  wire cfg_err_ecrc;
  wire cfg_err_cor;
  wire cfg_err_atomic_egress_blocked;
  wire cfg_err_internal_cor;
  wire cfg_err_malformed;
  wire cfg_err_mc_blocked;
  wire cfg_err_poisoned;
  wire cfg_err_norecovery;
  wire cfg_err_acs;
  wire cfg_err_internal_uncor;
  wire cfg_err_ur;
  wire cfg_err_cpl_timeout;
  wire cfg_err_cpl_abort;
  wire cfg_err_cpl_unexpect;
  wire cfg_err_posted;
  wire cfg_err_locked;
  wire [47:0] cfg_err_tlp_cpl_header;
  wire [127:0] cfg_err_aer_headerlog;
  wire [4:0] cfg_aer_interrupt_msgnum;
  wire cfg_interrupt;
  wire cfg_interrupt_assert;
  wire [7:0] cfg_interrupt_di;
  wire cfg_interrupt_stat;
  wire [4:0] cfg_pciecap_interrupt_msgnum;
  wire cfg_to_turnoff;
  wire [7:0] cfg_bus_number;
  wire [4:0] cfg_device_number;
  wire [2:0] cfg_function_number;
  wire [31:0] cfg_mgmt_di;
  wire [31:0] cfg_mgmt_do;
  wire [3:0] cfg_mgmt_byte_en;
  wire [9:0] cfg_mgmt_dwaddr;
  wire cfg_mgmt_wr_en;
  wire cfg_mgmt_rd_en;
  wire cfg_mgmt_wr_readonly;
  wire cfg_mgmt_rd_wr_done;  

  //
  // Physical layer control and status interface
  //
  wire pl_directed_link_auton;
  wire [1:0] pl_directed_link_change;
  wire pl_directed_link_speed;
  wire [1:0] pl_directed_link_width;
  wire pl_upstream_prefer_deemph;

  //
  // System interface
  //
  wire sys_rst_n_c;
  wire sys_clk;

  //
  // Register declaration
  //
  reg user_reset_q;
  reg user_lnk_up_q;

  //
  // Local parameters
  //
  localparam TCQ = 1;
  localparam USER_CLK_FREQ = 2;
  localparam USER_CLK2_DIV2 = "FALSE";
  localparam USERCLK2_FREQ = (USER_CLK2_DIV2 == "TRUE") ? (USER_CLK_FREQ == 4) ? 3 : (USER_CLK_FREQ == 3) ? 2 : USER_CLK_FREQ: USER_CLK_FREQ;
  
  // Reset input buffer
  IBUF sys_reset_n_ibuf(.O( sys_rst_n_c ), .I( sys_rst_n ));
  
  // Transciever clock input buffer
  IBUFDS_GTE2 refclk_ibuf(.O( sys_clk ), .I( sys_clk_p ), .IB( sys_clk_n ), .CEB( 1'b0 ));

  always @(posedge user_clk) begin
  
    user_reset_q <= user_reset;
    user_lnk_up_q <= user_lnk_up;
    
  end

  assign pipe_mmcm_rst_n = 1'b1;

  //
  // PCI Express endpoint shared logic wrapper
  //
  pcie_7x_0_support #
  (	 
    .LINK_CAP_MAX_LINK_WIDTH( 1 ),              // PCIe Lane Width
    .C_DATA_WIDTH( C_DATA_WIDTH ),              // RX/TX interface data width
    .KEEP_WIDTH( KEEP_WIDTH ),                  // TSTRB width
    .PCIE_REFCLK_FREQ( REF_CLK_FREQ ),          // PCIe reference clock frequency
    .PCIE_USERCLK1_FREQ( USER_CLK_FREQ + 1 ),   // PCIe user clock 1 frequency
    .PCIE_USERCLK2_FREQ( USERCLK2_FREQ + 1 ),   // PCIe user clock 2 frequency             
    .PCIE_USE_MODE("1.0"),                      // PCIe use mode
    .PCIE_GT_DEVICE("GTP")                      // PCIe GT device
  ) 
  pcie_7x_0_support_i
  (
    // PCI Express transmit
    .pci_exp_txn( pcie_txn ),
    .pci_exp_txp( pcie_txp ),

    // PCI Express receive
    .pci_exp_rxn( pcie_rxn ),
    .pci_exp_rxp( pcie_rxp ),

    // Clocking sharing    
    .pipe_pclk_sel_slave( 1'b0 ),
    .pipe_mmcm_rst_n( pipe_mmcm_rst_n ),

    // AXI-S Common
    .user_clk_out( user_clk ),
    .user_reset_out( user_reset ),
    .user_lnk_up( user_lnk_up ),

    // AXI-S transmit
    .s_axis_tx_tready( s_axis_tx_tready ),
    .s_axis_tx_tdata( s_axis_tx_tdata ),
    .s_axis_tx_tkeep( s_axis_tx_tkeep ),
    .s_axis_tx_tuser( s_axis_tx_tuser ),
    .s_axis_tx_tlast( s_axis_tx_tlast ),
    .s_axis_tx_tvalid( s_axis_tx_tvalid ),

    // AXI-S receive
    .m_axis_rx_tdata( m_axis_rx_tdata ),
    .m_axis_rx_tkeep( m_axis_rx_tkeep ),
    .m_axis_rx_tlast( m_axis_rx_tlast ),
    .m_axis_rx_tvalid( m_axis_rx_tvalid ),
    .m_axis_rx_tready( m_axis_rx_tready ),
    .m_axis_rx_tuser( m_axis_rx_tuser ),

    // Flow control   
    .fc_sel( fc_sel ),

    // Management interface
    .cfg_mgmt_di( cfg_mgmt_di ),
    .cfg_mgmt_do( cfg_mgmt_do ),
    .cfg_mgmt_byte_en( cfg_mgmt_byte_en ),
    .cfg_mgmt_dwaddr( cfg_mgmt_dwaddr ),
    .cfg_mgmt_wr_en( cfg_mgmt_wr_en ),
    .cfg_mgmt_rd_en( cfg_mgmt_rd_en ),
    .cfg_mgmt_wr_readonly( cfg_mgmt_wr_readonly ),
    .cfg_mgmt_rd_wr_done( cfg_mgmt_rd_wr_done ),
    .cfg_mgmt_wr_rw1c_as_rw( 1'b0 ),    

    // Error reporting interface
    .cfg_err_ecrc( cfg_err_ecrc ),
    .cfg_err_ur( cfg_err_ur ),
    .cfg_err_cpl_timeout( cfg_err_cpl_timeout ),
    .cfg_err_cpl_unexpect( cfg_err_cpl_unexpect ),
    .cfg_err_cpl_abort( cfg_err_cpl_abort ),
    .cfg_err_posted( cfg_err_posted ),
    .cfg_err_cor( cfg_err_cor ),
    .cfg_err_atomic_egress_blocked( cfg_err_atomic_egress_blocked ),
    .cfg_err_internal_cor( cfg_err_internal_cor ),
    .cfg_err_malformed( cfg_err_malformed ),
    .cfg_err_mc_blocked( cfg_err_mc_blocked ),
    .cfg_err_poisoned( cfg_err_poisoned ),
    .cfg_err_norecovery( cfg_err_norecovery ),
    .cfg_err_tlp_cpl_header( cfg_err_tlp_cpl_header ),
    .cfg_err_locked( cfg_err_locked ),
    .cfg_err_acs( cfg_err_acs ),
    .cfg_err_internal_uncor( cfg_err_internal_uncor ),

    // AER interface 
    .cfg_err_aer_headerlog( cfg_err_aer_headerlog ),
    .cfg_aer_interrupt_msgnum( cfg_aer_interrupt_msgnum ),

    // AXI common
    .tx_cfg_gnt( tx_cfg_gnt ),
    .rx_np_ok( rx_np_ok ),
    .rx_np_req( rx_np_req ),
    .cfg_trn_pending( cfg_trn_pending ),
    .cfg_pm_halt_aspm_l0s( cfg_pm_halt_aspm_l0s ),
    .cfg_pm_halt_aspm_l1( cfg_pm_halt_aspm_l1 ),
    .cfg_pm_force_state_en( cfg_pm_force_state_en ),
    .cfg_pm_force_state( cfg_pm_force_state ),
    .cfg_dsn( cfg_dsn ),
    .cfg_turnoff_ok( cfg_turnoff_ok ),
    .cfg_pm_wake( cfg_pm_wake ),
  
    // RP only
    .cfg_pm_send_pme_to( 1'b0 ),
    .cfg_ds_bus_number( 8'b0 ),
    .cfg_ds_device_number( 5'b0 ),
    .cfg_ds_function_number( 3'b0 ),
    
    // EP Only
    .cfg_interrupt( cfg_interrupt ),
    .cfg_interrupt_assert( cfg_interrupt_assert ),
    .cfg_interrupt_di( cfg_interrupt_di ),    
    .cfg_interrupt_stat( cfg_interrupt_stat ),
    .cfg_pciecap_interrupt_msgnum( cfg_pciecap_interrupt_msgnum ),

    // Configuration interface     
    .cfg_to_turnoff( cfg_to_turnoff ),
    .cfg_bus_number( cfg_bus_number ),
    .cfg_device_number( cfg_device_number ),
    .cfg_function_number( cfg_function_number ),    

    // Physical layer control and status interface
    .pl_directed_link_change( pl_directed_link_change ),
    .pl_directed_link_width( pl_directed_link_width ),
    .pl_directed_link_speed( pl_directed_link_speed ),
    .pl_directed_link_auton( pl_directed_link_auton ),
    .pl_upstream_prefer_deemph( pl_upstream_prefer_deemph ),    
    .pl_transmit_hot_rst( 1'b0 ),
    .pl_downstream_deemph_source( 1'b0 ),

    // PCI Express DRP interface
    .pcie_drp_clk( 1'b1 ),
    .pcie_drp_en( 1'b0 ),
    .pcie_drp_we( 1'b0 ),
    .pcie_drp_addr( 9'h0 ),
    .pcie_drp_di( 16'h0 ),

    // System interface
    .sys_clk( sys_clk ),
    .sys_rst_n( sys_rst_n_c )
  );

  assign fc_sel = 3'b0;

  assign tx_cfg_gnt = 1'b1;                        // Always allow transmission of Config traffic within block
  assign rx_np_ok = 1'b1;                          // Allow Reception of Non-posted Traffic
  assign rx_np_req = 1'b1;                         // Always request Non-posted Traffic if available
  assign cfg_pm_wake = 1'b0;                       // Never direct the core to send a PM_PME Message
  assign cfg_trn_pending = 1'b0;                   // Never set the transaction pending bit in the Device Status Register
  assign cfg_pm_halt_aspm_l0s = 1'b0;              // Allow entry into L0s
  assign cfg_pm_halt_aspm_l1 = 1'b0;               // Allow entry into L1
  assign cfg_pm_force_state_en  = 1'b0;            // Do not qualify cfg_pm_force_state
  assign cfg_pm_force_state  = 2'b00;              // Do not move force core into specific PM state  
  assign s_axis_tx_tuser[0] = 1'b0;                // Unused for V6
  assign s_axis_tx_tuser[1] = 1'b0;                // Error forward packet
  assign s_axis_tx_tuser[2] = 1'b0;                // Stream packet

  assign cfg_err_cor = 1'b0;                       // Never report Correctable Error
  assign cfg_err_ur = 1'b0;                        // Never report UR
  assign cfg_err_ecrc = 1'b0;                      // Never report ECRC Error
  assign cfg_err_cpl_timeout = 1'b0;               // Never report Completion Timeout
  assign cfg_err_cpl_abort = 1'b0;                 // Never report Completion Abort
  assign cfg_err_cpl_unexpect = 1'b0;              // Never report unexpected completion
  assign cfg_err_posted = 1'b0;                    // Never qualify cfg_err_* inputs
  assign cfg_err_locked = 1'b0;                    // Never qualify cfg_err_ur or cfg_err_cpl_abort
  assign cfg_err_atomic_egress_blocked = 1'b0;     // Never report Atomic TLP blocked
  assign cfg_err_internal_cor = 1'b0;              // Never report internal error occurred
  assign cfg_err_malformed = 1'b0;                 // Never report malformed error
  assign cfg_err_mc_blocked = 1'b0;                // Never report multi-cast TLP blocked
  assign cfg_err_poisoned = 1'b0;                  // Never report poisoned TLP received
  assign cfg_err_norecovery = 1'b0;                // Never qualify cfg_err_poisoned or cfg_err_cpl_timeout
  assign cfg_err_acs = 1'b0;                       // Never report an ACS violation
  assign cfg_err_internal_uncor = 1'b0;            // Never report internal uncorrectable error
  assign cfg_err_aer_headerlog = 128'h0;           // Zero out the AER Header Log
  assign cfg_aer_interrupt_msgnum = 5'b00000;      // Zero out the AER Root Error Status Register
  assign cfg_err_tlp_cpl_header = 48'h0;           // Zero out the header information

  assign cfg_interrupt_stat = 1'b0;                // Never set the Interrupt Status bit
  assign cfg_pciecap_interrupt_msgnum = 5'b00000;  // Zero out Interrupt Message Number
  assign cfg_interrupt_assert = 1'b0;              // Always drive interrupt de-assert
  assign cfg_interrupt = 1'b0;                     // Never drive interrupt by qualifying cfg_interrupt_assert
  assign cfg_interrupt_di = 8'b0;                  // Do not set interrupt fields

  assign pl_directed_link_change = 2'b00;          // Never initiate link change
  assign pl_directed_link_width = 2'b00;           // Zero out directed link width
  assign pl_directed_link_speed = 1'b0;            // Zero out directed link speed
  assign pl_directed_link_auton = 1'b0;            // Zero out link autonomous input
  assign pl_upstream_prefer_deemph = 1'b1;         // Zero out preferred de-emphasis of upstream port

  assign cfg_mgmt_di = 32'h0;                      // Zero out CFG MGMT input data bus
  assign cfg_mgmt_byte_en = 4'h0;                  // Zero out CFG MGMT byte enables
  assign cfg_mgmt_wr_en = 1'b0;                    // Do not write CFG space
  assign cfg_mgmt_wr_readonly = 1'b0;              // Never treat RO bit as RW  

  // Assign the input DSN
  assign cfg_dsn = { `PCI_EXP_EP_DSN_2, 
                     `PCI_EXP_EP_DSN_1 };   
                   
  // Tx
  assign s_axis_tx_tuser = 0;
  
  reg [21:0] debounce = 0;
  reg [1:0] user_io_0 = 0;
  reg [1:0] user_io_1 = 0;
  
  //
  // debounce push buttons
  //
  always @(posedge sys_clk) begin
  
    // decrement counter
    debounce <= debounce - 1;
    
    if (debounce != 0) begin
    
      user_io_0[0] <= user_io[0] ? 1 : user_io_0[0];
      user_io_0[1] <= user_io[1] ? 1 : user_io_0[1];
      
    end else begin  
      
      user_io_1 <= user_io_0;
      user_io_0 <= 2'b00;
      
    end  
  end     
  
  wire [15:0] device_id;
  wire device_id_ready;

  // completer address
  assign device_id = { cfg_bus_number, cfg_device_number, cfg_function_number };

  assign device_id_ready = (cfg_bus_number == 0 && 
                            cfg_device_number == 0 && 
                            cfg_function_number == 0) ? 0 : 1;
 
  //
  // Microblaze common I/O
  //
  wire clk_out;
  wire [31:0] gpio_in;
  wire [31:0] gpio_out;
  
  // combined reset signal
  assign reset_n = sys_rst_n_c & ~user_io_1[1]; 
  
  // GPIO input
  assign gpio_in = { 15'h0, user_io_1[0], device_id };
  
  // LEDs
  assign user_led = ~{ gpio_out[0], user_lnk_up, device_id_ready }; 
  
  //
  // Quad SPI signals
  //
  wire spi_io0_i;
  wire spi_io0_o;
  wire spi_io0_t;  
  wire spi_io1_i;
  wire spi_io1_o;
  wire spi_io1_t;  
  wire spi_io2_i;
  wire spi_io2_o;
  wire spi_io2_t;  
  wire spi_io3_i;
  wire spi_io3_o;
  wire spi_io3_t;  
  wire spi_ss_i;
  wire spi_ss_o;
  wire spi_ss_t;  

  // SPI buffers  
  IOBUF spi_io0_iobuf(.I( spi_io0_o ), .IO( spi_io0_io ), .O( spi_io0_i ), .T( spi_io0_t ));
  IOBUF spi_io1_iobuf(.I( spi_io1_o ), .IO( spi_io1_io ), .O( spi_io1_i ), .T( spi_io1_t ));
  IOBUF spi_io2_iobuf(.I( spi_io2_o ), .IO( spi_io2_io ), .O( spi_io2_i ), .T( spi_io2_t ));
  IOBUF spi_io3_iobuf(.I( spi_io3_o ), .IO( spi_io3_io ), .O( spi_io3_i ), .T( spi_io3_t ));
  IOBUF spi_ss_iobuf(.I( spi_ss_o ), .IO( spi_ss_io ), .O( spi_ss_i ), .T( spi_ss_t ));  
  
  //
  // AXI DMA transmit
  //
  wire [63:0] M_AXIS_MM2S_0_tdata;
  wire [7:0] M_AXIS_MM2S_0_tkeep;
  wire M_AXIS_MM2S_0_tlast;
  wire M_AXIS_MM2S_0_tready;
  wire M_AXIS_MM2S_0_tvalid;
  
  //
  // AXI DMA receive
  //
  wire [63:0] S_AXIS_S2MM_0_tdata;
  wire [7:0] S_AXIS_S2MM_0_tkeep;
  wire S_AXIS_S2MM_0_tlast;
  wire S_AXIS_S2MM_0_tready;
  wire S_AXIS_S2MM_0_tvalid;  
  
  //
  // TLP receive FIFO
  //
  axis_data_fifo_0 fifo_tlp_rx_i(
    .s_axis_aresetn( ~user_reset ),
    .s_axis_aclk( user_clk ),
    .s_axis_tvalid( m_axis_rx_tvalid ),
    .s_axis_tready( m_axis_rx_tready ),
    .s_axis_tdata( m_axis_rx_tdata ),
    .s_axis_tkeep( m_axis_rx_tkeep ),
    .s_axis_tlast( m_axis_rx_tlast ),
    .m_axis_aclk( clk_out ),
    .m_axis_tvalid( S_AXIS_S2MM_0_tvalid ),
    .m_axis_tready( S_AXIS_S2MM_0_tready ),
    .m_axis_tdata( S_AXIS_S2MM_0_tdata ),
    .m_axis_tkeep( S_AXIS_S2MM_0_tkeep ),
    .m_axis_tlast( S_AXIS_S2MM_0_tlast )
  );

  //
  // TLP transmit FIFO
  //
  axis_data_fifo_0 fifo_tlp_tx_i(
    .s_axis_aresetn( ~user_reset ),
    .s_axis_aclk( clk_out ),
    .s_axis_tvalid( M_AXIS_MM2S_0_tvalid ),
    .s_axis_tready( M_AXIS_MM2S_0_tready ),
    .s_axis_tdata( M_AXIS_MM2S_0_tdata ),
    .s_axis_tkeep( M_AXIS_MM2S_0_tkeep ),
    .s_axis_tlast( M_AXIS_MM2S_0_tlast ),
    .m_axis_aclk( user_clk ),
    .m_axis_tvalid( s_axis_tx_tvalid ),
    .m_axis_tready( s_axis_tx_tready ),
    .m_axis_tdata( s_axis_tx_tdata ),
    .m_axis_tkeep( s_axis_tx_tkeep ),
    .m_axis_tlast( s_axis_tx_tlast )
  );
  
  //
  // Config space access data in
  //  
  wire [31:0] S0_AXIS_0_tdata;
  wire S0_AXIS_0_tlast;
  wire S0_AXIS_0_tready;
  wire S0_AXIS_0_tvalid;

  reg s_axis_cfg_rx_tlast = 1'b1;
  
  //
  // Config space access data out
  //
  wire [31:0] M0_AXIS_0_tdata;
  wire M0_AXIS_0_tlast;
  wire M0_AXIS_0_tready;
  wire M0_AXIS_0_tvalid;

  wire [31:0] m_axis_cfg_tx_tdata;

  reg m_axis_cfg_tx_tready = 1'b1;
  
  assign cfg_mgmt_dwaddr = m_axis_cfg_tx_tdata[9:0];

  //
  // Config space data in FIFO
  //
  axis_data_fifo_1 fifo_cfg_rx_i(
    .s_axis_aresetn( ~user_reset ),    
    .s_axis_aclk( user_clk ),    
    .s_axis_tvalid( cfg_mgmt_rd_wr_done ),
    .s_axis_tdata( cfg_mgmt_do ),
    .s_axis_tlast( s_axis_cfg_rx_tlast ),
    .m_axis_aclk( clk_out ),
    .m_axis_tvalid( S0_AXIS_0_tvalid ),
    .m_axis_tready( S0_AXIS_0_tready ),
    .m_axis_tdata( S0_AXIS_0_tdata ),
    .m_axis_tlast( S0_AXIS_0_tlast )
  );     

  //
  // Config space data out FIFO
  //
  axis_data_fifo_1 fifo_cfg_tx_i(        
    .s_axis_aresetn( ~user_reset ),
    .s_axis_aclk( clk_out ),
    .s_axis_tvalid( M0_AXIS_0_tvalid ),
    .s_axis_tready( M0_AXIS_0_tready ),
    .s_axis_tdata( M0_AXIS_0_tdata ),
    .s_axis_tlast( M0_AXIS_0_tlast ),
    .m_axis_aclk( user_clk ),
    .m_axis_tvalid( cfg_mgmt_rd_en ),    
    .m_axis_tdata( m_axis_cfg_tx_tdata ),
    .m_axis_tready( m_axis_cfg_tx_tready )
  );

  //
  // Microblaze instance
  //
  microblaze microblaze_i(
    .M_AXIS_MM2S_0_tdata( M_AXIS_MM2S_0_tdata ),
    .M_AXIS_MM2S_0_tkeep( M_AXIS_MM2S_0_tkeep ),
    .M_AXIS_MM2S_0_tlast( M_AXIS_MM2S_0_tlast ),
    .M_AXIS_MM2S_0_tready( M_AXIS_MM2S_0_tready ),
    .M_AXIS_MM2S_0_tvalid( M_AXIS_MM2S_0_tvalid ),
    .S_AXIS_S2MM_0_tdata( S_AXIS_S2MM_0_tdata ),
    .S_AXIS_S2MM_0_tkeep( S_AXIS_S2MM_0_tkeep ),
    .S_AXIS_S2MM_0_tlast( S_AXIS_S2MM_0_tlast ),
    .S_AXIS_S2MM_0_tready( S_AXIS_S2MM_0_tready ),
    .S_AXIS_S2MM_0_tvalid( S_AXIS_S2MM_0_tvalid ),
    .M0_AXIS_0_tdata( M0_AXIS_0_tdata ),
    .M0_AXIS_0_tlast( M0_AXIS_0_tlast ),
    .M0_AXIS_0_tready( M0_AXIS_0_tready ),
    .M0_AXIS_0_tvalid( M0_AXIS_0_tvalid ),
    .S0_AXIS_0_tdata( S0_AXIS_0_tdata ),
    .S0_AXIS_0_tlast( S0_AXIS_0_tlast ),
    .S0_AXIS_0_tready( S0_AXIS_0_tready ),
    .S0_AXIS_0_tvalid( S0_AXIS_0_tvalid ),
    .sys_clk( sys_clk ),
    .sys_rst_n( reset_n ),
    .clk_out( clk_out ), 
    .uart_rxd( uart_rxd ),
    .uart_txd( uart_txd ),
    .gpio_in_tri_i( gpio_in ),
    .gpio_out_tri_o( gpio_out ),
    .spi_io0_i( spi_io0_i ),
    .spi_io0_o( spi_io0_o ),
    .spi_io0_t( spi_io0_t ),
    .spi_io1_i( spi_io1_i ),
    .spi_io1_o( spi_io1_o ),
    .spi_io1_t( spi_io1_t ),
    .spi_io2_i( spi_io2_i ),
    .spi_io2_o( spi_io2_o ),
    .spi_io2_t( spi_io2_t ),
    .spi_io3_i( spi_io3_i ),
    .spi_io3_o( spi_io3_o ),
    .spi_io3_t( spi_io3_t ),
    .spi_ss_i( spi_ss_i_0 ),
    .spi_ss_o( spi_ss_o_0 ),
    .spi_ss_t( spi_ss_t )    
  );
 
endmodule
