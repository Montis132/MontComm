import config from "@/config"
import http from "@/utils/request"

export default {
	register: {
		url: `${config.API_URL}/v1/register`,
		name: "用户注册",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	roleRegister: {
		url: `${config.API_URL}/v1/register/role`,
		name: "角色注册",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	certExport: {
		url: `${config.API_URL}/v1/cert/export`,
		name: "证书导出",
		get: async function(){
			return await http.get(this.url);
		}
	},
	keyGenerate: {
		url: `${config.API_URL}/v1/key/generate`,
		name: "密钥生成",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	serverList: {
		url: `${config.API_URL}/v1/server/list`,
		name: "服务器列表",
		get: async function(params={}){
			return await http.get(this.url, params);
		}
	},
	serverToggle: {
		url: `${config.API_URL}/v1/server/toggle`,
		name: "服务器启停",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	domainList: {
		url: `${config.API_URL}/v1/domain/list`,
		name: "域列表",
		get: async function(params={}){
			return await http.get(this.url, params);
		}
	},
	domainAdd: {
		url: `${config.API_URL}/v1/domain/add`,
		name: "新增域",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	domainEdit: {
		url: `${config.API_URL}/v1/domain/edit`,
		name: "编辑域",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	domainDelete: {
		url: `${config.API_URL}/v1/domain/delete`,
		name: "删除域",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	},
	keyImport: {
		url: `${config.API_URL}/v1/key/import`,
		name: "密钥导入",
		post: async function(data={}){
			return await http.post(this.url, data);
		}
	}
}
