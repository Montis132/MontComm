<template>
	<el-container>
		<el-header>
			<div class="left-panel">
				<el-button type="primary" icon="el-icon-plus" @click="add"></el-button>
				<el-button type="danger" plain icon="el-icon-delete" :disabled="selection.length==0" @click="batchDel"></el-button>
			</div>
			<div class="right-panel">
				<el-input v-model="searchKey" placeholder="搜索域名" clearable style="width:200px" @keyup.enter="search"></el-input>
				<el-button type="primary" icon="el-icon-search" @click="search"></el-button>
			</div>
		</el-header>
		<el-main class="nopadding">
			<scTable ref="table" :apiObj="list.apiObj" row-key="id" :params="searchParams" @selection-change="selectionChange" stripe>
				<el-table-column type="selection" width="50"></el-table-column>
				<el-table-column label="域名" prop="domainName" width="200"></el-table-column>
				<el-table-column label="成员列表" prop="members" min-width="300">
					<template #default="scope">
						<el-tag v-for="item in scope.row.members" :key="item" style="margin-right:5px;margin-bottom:3px">{{ item }}</el-tag>
					</template>
				</el-table-column>
				<el-table-column label="创建时间" prop="createTime" width="180"></el-table-column>
				<el-table-column label="操作" fixed="right" align="right" width="200">
					<template #default="scope">
						<el-button type="primary" plain size="small" @click="edit(scope.row)">编辑</el-button>
						<el-popconfirm title="确定删除吗？" @confirm="del(scope.row, scope.$index)">
							<template #reference>
								<el-button plain type="danger" size="small">删除</el-button>
							</template>
						</el-popconfirm>
					</template>
				</el-table-column>
			</scTable>
		</el-main>
	</el-container>

	<el-dialog :title="dialogTitle" v-model="dialogVisible" :width="600" destroy-on-close @closed="dialogVisible=false">
		<el-form ref="dialogForm" :model="form" :rules="rules" label-width="100px">
			<el-form-item label="域名" prop="domainName">
				<el-input v-model="form.domainName" placeholder="请输入域名"></el-input>
			</el-form-item>
			<el-form-item label="成员" prop="members">
				<el-select v-model="form.members" multiple filterable allow-create default-first-option placeholder="请输入成员" style="width:100%"></el-select>
			</el-form-item>
		</el-form>
		<template #footer>
			<el-button @click="dialogVisible=false">取消</el-button>
			<el-button type="primary" :loading="isSaving" @click="save">保存</el-button>
		</template>
	</el-dialog>
</template>

<script>
export default {
	data() {
		return {
			dialogVisible: false,
			dialogTitle: "新增域",
			isSaving: false,
			isEdit: false,
			searchKey: "",
			searchParams: {},
			selection: [],
			list: {
				apiObj: this.$API.commmngr.domainList
			},
			form: {
				domainName: "",
				members: []
			},
			rules: {
				domainName: [
					{ required: true, message: '请输入域名' }
				]
			}
		}
	},
	methods: {
		search() {
			this.searchParams = { key: this.searchKey }
			this.$refs.table.refresh()
		},
		add() {
			this.isEdit = false
			this.dialogTitle = "新增域"
			this.dialogVisible = true
			this.$nextTick(() => {
				this.$refs.dialogForm.resetFields()
			})
		},
		edit(row) {
			this.isEdit = true
			this.dialogTitle = "编辑域"
			this.dialogVisible = true
			this.$nextTick(() => {
				this.$refs.dialogForm.resetFields()
				this.form.domainName = row.domainName
				this.form.members = row.members || []
			})
		},
		async save() {
			var valid = await this.$refs.dialogForm.validate().catch(() => {})
			if (!valid) return
			this.isSaving = true
			var res
			if (this.isEdit) {
				res = await this.$API.commmngr.domainEdit.post(this.form)
			} else {
				res = await this.$API.commmngr.domainAdd.post(this.form)
			}
			this.isSaving = false
			if (res && res.code == 200) {
				this.$message.success("保存成功")
				this.dialogVisible = false
				this.$refs.table.refresh()
			} else {
				this.$alert(res.message, "提示", { type: 'error' })
			}
		},
		async del(row, index) {
			var res = await this.$API.commmngr.domainDelete.post({ id: row.id })
			if (res && res.code == 200) {
				this.$refs.table.removeIndex(index)
				this.$message.success("删除成功")
			} else {
				this.$alert(res.message, "提示", { type: 'error' })
			}
		},
		async batchDel() {
			var confirmRes = await this.$confirm(`确定删除选中的 ${this.selection.length} 项吗？`, '提示', {
				type: 'warning',
				confirmButtonText: '删除',
				confirmButtonClass: 'el-button--danger'
			}).catch(() => {})
			if (!confirmRes) return
			for (var i = 0; i < this.selection.length; i++) {
				await this.$API.commmngr.domainDelete.post({ id: this.selection[i].id })
			}
			this.$refs.table.refresh()
			this.$message.success("操作成功")
		},
		selectionChange(selection) {
			this.selection = selection
		}
	}
}
</script>
