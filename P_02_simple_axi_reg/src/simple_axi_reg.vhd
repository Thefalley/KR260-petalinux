library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

-- simple_axi_reg: 4 registros AXI-Lite de 32 bits.
--   reg0 (0x00): RW  - scratch register (escribe, lee)
--   reg1 (0x04): RW  - scratch register
--   reg2 (0x08): RW  - scratch register
--   reg3 (0x0C): RO  - id register, siempre 0xBADC0DE5
--
-- Util como "hello world" hardware: verifica que PL programado,
-- AXI-Lite del PS llega, y podemos leer/escribir sin DMA ni streams.

entity simple_axi_reg is
    generic (
        C_S_AXI_DATA_WIDTH : integer := 32;
        C_S_AXI_ADDR_WIDTH : integer := 4
    );
    port (
        S_AXI_ACLK    : in  std_logic;
        S_AXI_ARESETN : in  std_logic;
        S_AXI_AWADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
        S_AXI_AWPROT  : in  std_logic_vector(2 downto 0);
        S_AXI_AWVALID : in  std_logic;
        S_AXI_AWREADY : out std_logic;
        S_AXI_WDATA   : in  std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
        S_AXI_WSTRB   : in  std_logic_vector((C_S_AXI_DATA_WIDTH/8)-1 downto 0);
        S_AXI_WVALID  : in  std_logic;
        S_AXI_WREADY  : out std_logic;
        S_AXI_BRESP   : out std_logic_vector(1 downto 0);
        S_AXI_BVALID  : out std_logic;
        S_AXI_BREADY  : in  std_logic;
        S_AXI_ARADDR  : in  std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
        S_AXI_ARPROT  : in  std_logic_vector(2 downto 0);
        S_AXI_ARVALID : in  std_logic;
        S_AXI_ARREADY : out std_logic;
        S_AXI_RDATA   : out std_logic_vector(C_S_AXI_DATA_WIDTH-1 downto 0);
        S_AXI_RRESP   : out std_logic_vector(1 downto 0);
        S_AXI_RVALID  : out std_logic;
        S_AXI_RREADY  : in  std_logic
    );
end simple_axi_reg;

architecture rtl of simple_axi_reg is
    signal reg0, reg1, reg2 : std_logic_vector(31 downto 0) := (others => '0');
    constant ID_REG         : std_logic_vector(31 downto 0) := X"BADC0DE5";

    signal awready, wready, bvalid : std_logic := '0';
    signal arready, rvalid         : std_logic := '0';
    signal rdata                   : std_logic_vector(31 downto 0) := (others => '0');
    signal waddr                   : std_logic_vector(C_S_AXI_ADDR_WIDTH-1 downto 0);
begin
    S_AXI_AWREADY <= awready;
    S_AXI_WREADY  <= wready;
    S_AXI_BVALID  <= bvalid;
    S_AXI_BRESP   <= "00";
    S_AXI_ARREADY <= arready;
    S_AXI_RVALID  <= rvalid;
    S_AXI_RDATA   <= rdata;
    S_AXI_RRESP   <= "00";

    -- Write handshake (simplificado, un write a la vez)
    process(S_AXI_ACLK)
    begin
        if rising_edge(S_AXI_ACLK) then
            if S_AXI_ARESETN = '0' then
                awready <= '0'; wready <= '0'; bvalid <= '0';
                reg0 <= (others=>'0'); reg1 <= (others=>'0'); reg2 <= (others=>'0');
            else
                -- Aceptar direccion y dato al mismo tiempo (simple)
                if awready = '0' and wready = '0' and S_AXI_AWVALID = '1' and S_AXI_WVALID = '1' then
                    awready <= '1'; wready <= '1';
                    waddr   <= S_AXI_AWADDR;
                else
                    awready <= '0'; wready <= '0';
                end if;

                -- Escribir registro seleccionado en el ciclo siguiente
                if awready = '1' and wready = '1' then
                    case waddr(3 downto 2) is
                        when "00" => reg0 <= S_AXI_WDATA;
                        when "01" => reg1 <= S_AXI_WDATA;
                        when "10" => reg2 <= S_AXI_WDATA;
                        when others => null;  -- reg3 es RO
                    end case;
                    bvalid <= '1';
                elsif bvalid = '1' and S_AXI_BREADY = '1' then
                    bvalid <= '0';
                end if;
            end if;
        end if;
    end process;

    -- Read handshake
    process(S_AXI_ACLK)
    begin
        if rising_edge(S_AXI_ACLK) then
            if S_AXI_ARESETN = '0' then
                arready <= '0'; rvalid <= '0'; rdata <= (others => '0');
            else
                if arready = '0' and S_AXI_ARVALID = '1' and rvalid = '0' then
                    arready <= '1';
                    case S_AXI_ARADDR(3 downto 2) is
                        when "00" => rdata <= reg0;
                        when "01" => rdata <= reg1;
                        when "10" => rdata <= reg2;
                        when "11" => rdata <= ID_REG;
                        when others => rdata <= (others=>'0');
                    end case;
                else
                    arready <= '0';
                end if;

                if arready = '1' then
                    rvalid <= '1';
                elsif rvalid = '1' and S_AXI_RREADY = '1' then
                    rvalid <= '0';
                end if;
            end if;
        end if;
    end process;
end rtl;
