#include <stdio.h>
#include <stdlib.h>
#include <codecvt>
#include <locale>
#include "ResourceTypes.h"

void readResTableHeader(FILE* pFile);

void printUtf16String(char16_t* str16);

void readPackageResource(FILE* pFile, struct ResTable_package header);

void printStringFromStringsPool(uint32_t* pOffsets, char* pStringsStart, uint32_t stringIndex, uint32_t isUtf8);

void printConfig(struct ResTable_config config);

void printStringOfComplex(uint32_t complex, bool isFraction);

void printValue(const Res_value* value, struct ResStringPool_header globalStringPoolHeader, unsigned char* pGlobalStrings);

struct ResStringPool_header readResStringPoolHeader(FILE* pFile);

unsigned char* readStringsFromStringPool(FILE* pFile,  struct ResStringPool_header header);

int readResTablePackageHeader(FILE* pFile, struct ResTable_package* pHeader);

int main(int argc, char *argv[]) {
	FILE* pFile = fopen(argv[1], "rb");

	printf("=====================RES_TABLE_TYPE=====================\n");
	readResTableHeader(pFile);

	printf("=====================RES_STRING_POOL_TYPE=====================\n");
	struct ResStringPool_header globalStringPoolHeader = readResStringPoolHeader(pFile);
	unsigned char* pGlobalStrings = readStringsFromStringPool(pFile, globalStringPoolHeader);
	struct ResTable_package packageHeader;

	printf("=====================RES_TABLE_PACKAGE_TYPE=====================\n");
	while(readResTablePackageHeader(pFile, &packageHeader)) {
		printf("=====================types=====================\n");
		struct ResStringPool_header typeStringPoolHeader = readResStringPoolHeader(pFile);
		unsigned char* pTypeStrings = readStringsFromStringPool(pFile, typeStringPoolHeader);

		printf("=====================keys=====================\n");
		struct ResStringPool_header keyStringPoolHeader = readResStringPoolHeader(pFile);
		unsigned char* pKeyStrings = readStringsFromStringPool(pFile, keyStringPoolHeader);

		/*
		struct ResChunk_header chunkHeader;
		uint8_t id;
		while(fread((void*)&chunkHeader, sizeof(struct ResChunk_header), 1, pFile)
				&& chunkHeader.type != RES_TABLE_PACKAGE_TYPE) {
			fread((void*)&id, sizeof(uint8_t), 1, pFile);
			printf("0x%x, %d\n", chunkHeader.type, id);
			fseek(pFile, chunkHeader.size - sizeof(struct ResChunk_header) - sizeof(uint8_t), SEEK_CUR);
		}
		continue;
		*/

		struct ResTable_type typeHeader;
		struct ResTable_typeSpec typeSpecHeader;
		uint32_t config;
		uint16_t type;
		while(fread((void*)&type, sizeof(u_int16_t), 1, pFile) != 0) {
			fseek(pFile, -sizeof(uint16_t), SEEK_CUR);
			if(RES_TABLE_TYPE_SPEC_TYPE == type) {
				fread((void*)&typeSpecHeader, sizeof(struct ResTable_typeSpec), 1, pFile);
				printf("type: id=0x%x,name=", typeSpecHeader.id);
				printStringFromStringsPool(
						(uint32_t*)pTypeStrings,
						(char*)pTypeStrings + typeStringPoolHeader.stringsStart - sizeof(struct ResStringPool_header),
						typeSpecHeader.id - 1,
						typeStringPoolHeader.flags & ResStringPool_header::UTF8_FLAG
				);

				for(int i = 0 ; i < typeSpecHeader.entryCount ; i++) {
					fread((void*)&config, sizeof(uint32_t), 1, pFile);
					printf("0x%x\n",config);
				}
			} else if(RES_TABLE_TYPE_TYPE == type) {
				fread((void*)&typeHeader, sizeof(struct ResTable_type), 1, pFile);
				printConfig(typeHeader.config);

				// 实际struct ResTable_type的大小可能不同sdk版本不一样,所以typeHeader.header.headerSize才是真正的头部大小
				fseek(pFile, typeHeader.header.headerSize -  sizeof(struct ResTable_type), SEEK_CUR);;

				uint32_t* pOffsets = (uint32_t*)malloc(typeHeader.entryCount * sizeof(uint32_t));
				fread((void*)pOffsets, sizeof(uint32_t), typeHeader.entryCount, pFile);

				unsigned char* pData = (unsigned char*)malloc(typeHeader.header.size - typeHeader.entriesStart);
				fread((void*)pData, typeHeader.header.size - typeHeader.entriesStart, 1, pFile);

				for(int i = 0 ; i< typeHeader.entryCount ; i++) {
					uint32_t offset = *(pOffsets + i);
					if(offset == ResTable_type::NO_ENTRY) {
						continue;
					}
					struct ResTable_entry* pEntry = (struct ResTable_entry*)(pData + offset);
					printf("entryIndex: 0x%x, key :\n", i);
					printStringFromStringsPool(
							(uint32_t*)pKeyStrings,
							(char*)pKeyStrings + keyStringPoolHeader.stringsStart - sizeof(struct ResStringPool_header),
							pEntry->key.index,
							keyStringPoolHeader.flags & ResStringPool_header::UTF8_FLAG
					);
					if(pEntry->flags & ResTable_entry::FLAG_COMPLEX) {
						struct ResTable_map_entry* pMapEntry = (struct ResTable_map_entry*)(pData + offset);
						for(int i = 0; i <pMapEntry->count ; i++) {
							struct ResTable_map* pMap =
							   	(struct ResTable_map*)(pData + offset + pMapEntry->size + i * sizeof(struct ResTable_map));
							printf("\tname:0x%08x, valueType:%u, value:%u\n",
									pMap->name.ident, pMap->value.dataType, pMap->value.data);
						}
					} else {
						struct Res_value* pValue = (struct Res_value*)((unsigned char*)pEntry + sizeof(struct ResTable_entry));
						printf("value :\n");
						printValue(pValue, globalStringPoolHeader, pGlobalStrings);
						printf("\n");
					}
				}
				free(pOffsets);
				free(pData);
			} else {
				break;
			}
		}
	}

	fclose(pFile);
	return 0;
}

void readResTableHeader(FILE* pFile) {
	uint16_t type;
	fread((void*)&type, sizeof(type), 1, pFile);
	if(type == RES_TABLE_TYPE) {
		struct ResTable_header header = {RES_TABLE_TYPE};
		fread((void*)(((char*)&header)+2), sizeof(struct ResTable_header)-2, 1, pFile);
		printf("type:%u, headSize:%u, size:%u, packageCount:%u\n",
				header.header.type,
				header.header.headerSize,
				header.header.size,
				header.packageCount);
	}
}

struct ResStringPool_header readResStringPoolHeader(FILE* pFile) {
	struct ResStringPool_header header;
	uint16_t type;
	fread((void*)&header, sizeof(struct ResStringPool_header), 1, pFile);
	printf("type:%u, headSize:%u, size:%u, stringCount:%u, stringStart:%u, styleCount:%u, styleStart:%u\n",
				header.header.type,
				header.header.headerSize,
				header.header.size,
				header.stringCount,
				header.stringsStart,
				header.styleCount,
				header.stylesStart);
	return header;
}

void printUtf16String(char16_t* str16) {
	printf("%s\n", std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>().to_bytes(str16).c_str());
}

unsigned char* readStringsFromStringPool(FILE* pFile,  struct ResStringPool_header header) {
	uint32_t size = header.header.size - sizeof(struct ResStringPool_header);
	unsigned char* pData = (unsigned char*)malloc(size);
	fread((void*)pData, size, 1, pFile);
	uint32_t* pOffsets = (uint32_t*)pData;

	//stringsStart指的是header的起始地址到字符串起始地址的距离
	//pData已经是header末尾的地址了，所以要减去header的大小
	char* pStringsStart = (char*)(pData + header.stringsStart - sizeof(struct ResStringPool_header));

	for(int i = 0 ; i < header.stringCount ; i++) {
		//前面两个字节是长度,要跳过
		char* str = pStringsStart + *(pOffsets + i) + 2;
		if(header.flags & ResStringPool_header::UTF8_FLAG) {
			printf("%s\n", str);
		} else {
			printUtf16String((char16_t*)str);
		}
	}

	return pData;
}

int readResTablePackageHeader(FILE* pFile, struct ResTable_package* pHeader) {
	if(fread((void*)pHeader, sizeof(struct ResTable_package), 1, pFile) == 0) {
		return 0;
	}
	printf("type:%u, headSize:%u, size:%u, id:%x, packageName:",
				pHeader->header.type,
				pHeader->header.headerSize,
				pHeader->header.size,
				pHeader->id);
	printUtf16String((char16_t*)pHeader->name);
	return 1;
}

void readPackageResource(FILE* pFile, struct ResTable_package header) {
	fseek(pFile, header.header.size - header.header.headerSize, SEEK_CUR);
}

void printStringFromStringsPool(uint32_t* pOffsets, char* pStringsStart, uint32_t stringIndex, uint32_t isUtf8) {
	//前面两个字节是长度,要跳过
	char* str = pStringsStart + *(pOffsets + stringIndex) + 2;
	if(isUtf8) {
		printf("%s\n", str);
	} else {
		printUtf16String((char16_t*)str);
	}
}

void printConfig(struct ResTable_config config) {
	std::string str = config.toString();
	printf("config : %s\n", str.c_str());
}

void printStringOfComplex(uint32_t complex, bool isFraction) {
    const float MANTISSA_MULT =
        1.0f / (1<<Res_value::COMPLEX_MANTISSA_SHIFT);
    const float RADIX_MULTS[] = {
        1.0f*MANTISSA_MULT, 1.0f/(1<<7)*MANTISSA_MULT,
        1.0f/(1<<15)*MANTISSA_MULT, 1.0f/(1<<23)*MANTISSA_MULT
    };

    float value = (complex&(Res_value::COMPLEX_MANTISSA_MASK
                   <<Res_value::COMPLEX_MANTISSA_SHIFT))
            * RADIX_MULTS[(complex>>Res_value::COMPLEX_RADIX_SHIFT)
                            & Res_value::COMPLEX_RADIX_MASK];
	printf("%f",value);

    if (!isFraction) {
        switch ((complex>>Res_value::COMPLEX_UNIT_SHIFT)&Res_value::COMPLEX_UNIT_MASK) {
            case Res_value::COMPLEX_UNIT_PX: printf("px\n"); break;
            case Res_value::COMPLEX_UNIT_DIP: printf("dp\n"); break;
            case Res_value::COMPLEX_UNIT_SP: printf("sp\n"); break;
            case Res_value::COMPLEX_UNIT_PT: printf("pt\n"); break;
            case Res_value::COMPLEX_UNIT_IN: printf("in\n"); break;
            case Res_value::COMPLEX_UNIT_MM: printf("mm\n"); break;
            default: printf("(unknown unit)\n"); break;
        }
    } else {
        switch ((complex>>Res_value::COMPLEX_UNIT_SHIFT)&Res_value::COMPLEX_UNIT_MASK) {
            case Res_value::COMPLEX_UNIT_FRACTION: printf("%%\n"); break;
            case Res_value::COMPLEX_UNIT_FRACTION_PARENT: printf("%%p\n"); break;
            default: printf("(unknown unit)\n"); break;
        }
    }
}

void printValue(const Res_value* value, struct ResStringPool_header globalStringPoolHeader, unsigned char* pGlobalStrings) {
    if (value->dataType == Res_value::TYPE_NULL) {
        printf("(null)\n");
    } else if (value->dataType == Res_value::TYPE_REFERENCE) {
		printf("(reference) 0x%08x\n",value->data);
    } else if (value->dataType == Res_value::TYPE_ATTRIBUTE) {
		printf("(attribute) 0x%08x\n",value->data);
    } else if (value->dataType == Res_value::TYPE_STRING) {
		printf("(string) ");
		printStringFromStringsPool(
				(uint32_t*)pGlobalStrings,
				(char*)pGlobalStrings + globalStringPoolHeader.stringsStart - sizeof(struct ResStringPool_header),
				value->data,
				globalStringPoolHeader.flags & ResStringPool_header::UTF8_FLAG
		);
    } else if (value->dataType == Res_value::TYPE_FLOAT) {
        printf("(float) %f\n",*(const float*)&value->data);
    } else if (value->dataType == Res_value::TYPE_DIMENSION) {
        printf("(dimension) ");
		printStringOfComplex(value->data, false);
    } else if (value->dataType == Res_value::TYPE_FRACTION) {
        printf("(fraction) ");
		printStringOfComplex(value->data, true);
    } else if (value->dataType >= Res_value::TYPE_FIRST_COLOR_INT
            && value->dataType <= Res_value::TYPE_LAST_COLOR_INT) {
		printf("(color) #%08x\n", value->data);
    } else if (value->dataType == Res_value::TYPE_INT_BOOLEAN) {
        printf("(boolean) %s\n",value->data ? "true" : "false");
    } else if (value->dataType >= Res_value::TYPE_FIRST_INT
            && value->dataType <= Res_value::TYPE_LAST_INT) {
        printf("(int) %d or %u\n", value->data, value->data);
    } else {
		printf("(unknown type)\n");
    }
}

