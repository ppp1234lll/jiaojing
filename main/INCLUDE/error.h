#ifndef _ERROR_H_
#define _ERROR_H_

/* 网络参数 */
#define INCORRECT_ACCOUNT_OR_PASSWORD_NUM (401)
#define INCORRECT_ACCOUNT_OR_PASSWORD_STR ("\"incorrect account or password!\"") // 密码或账号名称错误

#define PARAMETER_ERROR_NUM (101)
#define PARAMETER_ERROR_STR ("\"parameter error!\"") // 参数错误

// HTTPStatus
#define HTTP_OK 									(200)
#define HTTP_BAD_REQUEST 					(400)
#define HTTP_FORBIDDEN  					(403)
#define HTTP_NOT_FOUND 						(404)
#define HTTP_SERVICE_UNAVAILABLE  (500)

// 公共功能错误码 
#define PUBLIC_ERROR_OK    							("0x00000000")   // 成功
#define PUBLIC_ERROR_NOT_ACTIVATED 			("0x00100001")   // 设备没有激活
#define PUBLIC_ERROR_OPERATION_FAILED 	("0x00100002")   // 设备操作失败，原因是权限不够
#define PUBLIC_ERROR_NOT_SUPPORTED 			("0x00100003")   // 设备不支持该功能
#define PUBLIC_ERROR_RESOURCES 					("0x00100004")   // 设备运行资源不足
#define PUBLIC_ERROR_INCORRECT_PARAM 		("0x00100007")   // 参数错误


// 公共功能错误码
#define ERRORMSG_OK    								("Succeeded.")   														// 成功
#define ERRORMSG_NOT_ACTIVATED 				("The device is not activated.")   					// 设备没有激活
#define ERRORMSG_OPERATION_FAILED 		("Device operation failed. No permission.") // 设备操作失败，原因是权限不够
#define ERRORMSG_NOT_SUPPORTED 				("This function is not supported.")   			// 设备不支持该功能
#define ERRORMSG_RESOURCES 						("Insufficient resources.")   							// 设备运行资源不足
#define ERRORMSG_INCORRECT_PARAM 			("Incorrect parameter.")   									// 参数错误


// 维护功能模块错误码
#define MAINTAIN_ERROR_OK    					("0x00000000")   // 成功
#define ERROR_CODE_NOT_ACTIVATED 			("0x00100001")   // 设备没有激活
#define ERROR_CODE_OPERATION_FAILED 	("0x00100002")   // 设备操作失败，原因是权限不够
#define ERROR_CODE_NOT_SUPPORTED 			("0x00100003")   // 设备不支持该功能
#define ERROR_CODE_RESOURCES 					("0x00100004")   // 设备运行资源不足
#define ERROR_CODE_INCORRECT_PARAM 		("0x00100007")   // 参数错误



#define ERRORMSG_OK    								("Succeeded.")   														// 成功
#define ERRORMSG_NOT_ACTIVATED 				("The device is not activated.")   					// 设备没有激活
#define ERRORMSG_OPERATION_FAILED 		("Device operation failed. No permission.") // 设备操作失败，原因是权限不够
#define ERRORMSG_NOT_SUPPORTED 				("This function is not supported.")   			// 设备不支持该功能
#define ERRORMSG_RESOURCES 						("Insufficient resources.")   							// 设备运行资源不足
#define ERRORMSG_INCORRECT_PARAM 			("Incorrect parameter.")   									// 参数错误






#endif
