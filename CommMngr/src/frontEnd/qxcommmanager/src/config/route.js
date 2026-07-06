// 静态路由配置
// 书写格式与动态路由格式一致，全部经由框架统一转换
// 比较动态路由在meta中多加入了role角色权限，为数组类型。一个菜单是否有权限显示，取决于它以及后代菜单是否有权限。
// routes 显示在左侧菜单中的路由(显示顺序在动态路由之前)
// 示例如下

// const routes = [
// 	{
// 		name: "demo",
// 		path: "/demo",
// 		meta: {
// 			icon: "el-icon-eleme-filled",
// 			title: "演示",
// 			role: ["SA"]
// 		},
// 		children: [{
// 			name: "demopage",
// 			path: "/demopage",
// 			component: "test/autocode/index",
// 			meta: {
// 				icon: "el-icon-menu",
// 				title: "演示页面",
// 				role: ["SA"]
// 			}
// 		}]
// 	}
// ]

const routes = [
	{
		name: "register",
		path: "/register",
		meta: {
			icon: "el-icon-user-filled",
			title: "账号注册",
			role: ["manager"]
		},
		children: [{
			name: "registerAccount",
			path: "/register/account",
			component: "register/index",
			meta: {
				icon: "el-icon-plus",
				title: "注册账号",
				role: ["manager"]
			}
		}, {
			name: "roleRegister",
			path: "/register/role",
			component: "register/RoleRegister",
			meta: {
				icon: "el-icon-s-custom",
				title: "角色注册",
				role: ["manager"]
			}
		}]
	},
	{
		name: "cert",
		path: "/cert",
		meta: {
			icon: "el-icon-document-copy",
			title: "证书管理",
			role: ["partner"]
		},
		children: [{
			name: "exportCert",
			path: "/cert/export",
			component: "cert/ExportCert",
			meta: {
				icon: "el-icon-download",
				title: "证书导出",
				role: ["partner"]
			}
		}]
	},
	{
		name: "key",
		path: "/key",
		meta: {
			icon: "el-icon-key",
			title: "密钥管理",
			role: ["partner", "user"]
		},
		children: [{
			name: "generateKey",
			path: "/key/generate",
			component: "key/GenerateKey",
			meta: {
				icon: "el-icon-refresh",
				title: "密钥生成",
				role: ["partner", "user"]
			}
		}, {
			name: "importKey",
			path: "/key/import",
			component: "key/ImportKey",
			meta: {
				icon: "el-icon-upload",
				title: "SM2公钥导入",
				role: ["partner", "user"]
			}
		}]
	},
	{
		name: "monitor",
		path: "/monitor",
		meta: {
			icon: "el-icon-monitor",
			title: "监控管理",
			role: ["manager"]
		},
		children: [{
			name: "serverMonitor",
			path: "/monitor/server",
			component: "monitor/ServerMonitor",
			meta: {
				icon: "el-icon-data-line",
				title: "服务器监控",
				role: ["manager"]
			}
		}]
	},
	{
		name: "domain",
		path: "/domain",
		meta: {
			icon: "el-icon-connection",
			title: "域管理",
			role: ["partner"]
		},
		children: [{
			name: "domainManage",
			path: "/domain/manage",
			component: "domain/DomainManage",
			meta: {
				icon: "el-icon-setting",
				title: "互通域管理",
				role: ["partner"]
			}
		}]
	}
]

export default routes;
