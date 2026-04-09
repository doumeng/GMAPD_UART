#ifndef _SPI_CLASS__H___
#define _SPI_CLASS__H___

int spiDevInit(const char *devname, unsigned int  mode, unsigned int  speed, unsigned char bit_per);

int spidevTransfer(int fd, unsigned char *txbuf, unsigned char *rxbuf, size_t len);

int spidevDeInit(int fd);

enum uart_error_code 
{
    SDR_SUCCESS                 = 0,
    SDR_ERROR                   = -1,
    SDR_ERROR_ENOENT            = -2,
	SDR_ERROR_ENXIO             = -6, //设备不存在或无效地址
	SDR_ERROR_IO                = -6, 
    SDR_ERROR_ENOMEM            = -12,    
    SDR_ERROR_EFAULT            = -14,
	SDR_ERROR_CLOSE            = -6, 


    SDR_ERROR_EBUSY             = -16,

    SDR_ERROR_EINVAL            = -22,

    
    SDR_ERROR_ENOSYS            = -38,

    SDR_ERROR_ARG               = -100, 
    SDR_ERROR_OVERFLOW          = -101, //memory error
	SDR_ERROR_TIMEOUT           = -102,
};

typedef enum spi_mode
{
    CPOL0_CPHA0 = 0,
    CPOL0_CPHA1,
    CPOL1_CPHA0,
    CPOL1_CPHA1,
} spi_mode_t;

typedef struct sdr_spi_handler spi_handler_t;

spi_handler_t* sdr_spi_init(char* device_name, spi_mode_t mode, unsigned int max_speed_hz);
void		   sdr_spi_deinit(spi_handler_t* spi);

int sdr_spi_set_timming_mode(spi_handler_t* spi, spi_mode_t mode);
int sdr_spi_set_speed(spi_handler_t* spi, unsigned int max_speed_hz);

int sdr_spi_send_and_recv(spi_handler_t* spi, const unsigned char* txbuf, unsigned char* rxbuf, size_t len);
int sdr_spi_send_then_send(spi_handler_t* spi, const unsigned char* txbuf_frist, size_t txbuf_frist_len,
    const unsigned char* txbuf_secd, size_t txbuf_secd_len);

int sdr_spi_send_then_recv(
    spi_handler_t* spi, const unsigned char* txbuf, size_t txbuf_len, unsigned char* rxbuf, size_t rxbuf_len);

int sdr_spi_ioctl(spi_handler_t* spi, unsigned int cmd, void* data);

/* Error Handling */
int sdr_spi_errno(spi_handler_t* spi);
const char *sdr_spi_errmsg(spi_handler_t* spi);

#endif // !_SPI_CLASS__H___