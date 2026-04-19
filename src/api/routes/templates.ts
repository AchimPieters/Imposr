import { Router } from 'express';
import { TemplateController } from '../controllers/TemplateController';

const router = Router();
const controller = new TemplateController();

router.get('/', (req, res, next) => {
  void controller.list(req, res, next);
});

router.get('/:fileName', (req, res, next) => {
  void controller.load(req, res, next);
});

router.post('/', (req, res, next) => {
  void controller.save(req, res, next);
});

export default router;
