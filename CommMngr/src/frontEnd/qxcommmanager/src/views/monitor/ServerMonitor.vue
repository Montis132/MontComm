<template>
	<el-container>
		<el-header>
			<div class="left-panel">
				<el-button type="primary" icon="el-icon-refresh" @click="refresh">刷新</el-button>
			</div>
		</el-header>
		<el-main class="nopadding">
			<scTable ref="table" :apiObj="list.apiObj" stripe row-key="id" @data-change="dataChange">
				<el-table-column label="服务器名称" prop="name" width="180"></el-table-column>
				<el-table-column label="IP地址" prop="ip" width="150"></el-table-column>
				<el-table-column label="CPU使用率" prop="cpu" width="130">
					<template #default="scope">
						<el-progress :percentage="scope.row.cpu" :color="cpuColor(scope.row.cpu)"></el-progress>
					</template>
				</el-table-column>
				<el-table-column label="内存使用率" prop="memory" width="130">
					<template #default="scope">
						<el-progress :percentage="scope.row.memory" :color="memoryColor(scope.row.memory)"></el-progress>
					</template>
				</el-table-column>
				<el-table-column label="状态" prop="status" width="100">
					<template #default="scope">
						<el-tag v-if="scope.row.status=='online'" type="success">在线</el-tag>
						<el-tag v-else type="danger">离线</el-tag>
					</template>
				</el-table-column>
				<el-table-column label="启用" prop="enabled" width="80">
					<template #default="scope">
						<el-switch v-model="scope.row.enabled" @change="toggleEnabled(scope.row)"></el-switch>
					</template>
				</el-table-column>
				<el-table-column label="最后更新" prop="lastUpdate" width="180"></el-table-column>
			</scTable>
		</el-main>
	</el-container>
</template>

<script>
export default {
	data() {
		return {
			list: {
				apiObj: this.$API.commmngr.serverList
			}
		}
	},
	mounted() {
		this.startPolling()
	},
	beforeUnmount() {
		this.stopPolling()
	},
	methods: {
		cpuColor(val) {
			if (val < 50) return '#67C23A'
			if (val < 80) return '#E6A23C'
			return '#F56C6C'
		},
		memoryColor(val) {
			if (val < 50) return '#67C23A'
			if (val < 80) return '#E6A23C'
			return '#F56C6C'
		},
		async toggleEnabled(row) {
			var res = await this.$API.commmngr.serverToggle.post({ id: row.id, enabled: row.enabled })
			if (res && res.code == 200) {
				this.$message.success("操作成功")
			} else {
				row.enabled = !row.enabled
				this.$message.error("操作失败")
			}
		},
		dataChange(data) {
			this.tableData = data
		},
		refresh() {
			this.$refs.table.refresh()
		},
		startPolling() {
			this.pollingTimer = setInterval(() => {
				this.$refs.table.refresh()
			}, 10000)
		},
		stopPolling() {
			if (this.pollingTimer) {
				clearInterval(this.pollingTimer)
			}
		}
	}
}
</script>
