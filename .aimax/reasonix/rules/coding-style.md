# 编码风格

## 不可变性（关键）

始终创建新对象，绝不修改原对象：

```javascript
// 错误: 可变操作
function updateUser(user, name) {
  user.name = name  // 可变操作！
  return user
}

// 正确: 不可变操作
function updateUser(user, name) {
  return {
    ...user,
    name
  }
}
```

## 文件组织

多个小文件 > 少量大文件：
- 高内聚，低耦合
- 通常 200-400 行，最多 800 行
- 从大型组件中提取工具函数
- 按功能/领域组织，而非按类型组织

## 错误处理

始终全面处理错误：

```typescript
try {
  const result = await riskyOperation()
  return result
} catch (error) {
  console.error('Operation failed:', error)
  throw new Error('Detailed user-friendly message')
}
```

## 输入验证

始终验证用户输入：

```typescript
import { z } from 'zod'

const schema = z.object({
  email: z.string().email(),
  age: z.number().int().min(0).max(150)
})

const validated = schema.parse(input)
```

## 代码质量检查清单

在标记工作完成前：
- [ ] 代码可读性好且命名规范
- [ ] 函数简短（<50 行）
- [ ] 文件聚焦（<800 行）
- [ ] 无深层嵌套（>4 层）
- [ ] 正确的错误处理
- [ ] 无 console.log 语句
- [ ] 无硬编码值
- [ ] 无可变操作（使用不可变模式）
