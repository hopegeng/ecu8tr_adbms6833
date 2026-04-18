#ifndef BMU_TYPES_H
#define BMU_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    BMU_OK = 0,
    BMU_E_NOT_OK,
    BMU_E_PARAM,
    BMU_E_RANGE,
    BMU_E_BUSY
} Bmu_ReturnType;

#endif /* BMU_TYPES_H */
